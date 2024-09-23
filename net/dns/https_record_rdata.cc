// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

bool ReadNextServiceParam(std::optional<uint16_t> last_key,
                          base::SpanReader<const uint8_t>& reader,
                          uint16_t* out_param_key,
                          std::string_view* out_param_value) {
  DCHECK(out_param_key);
  DCHECK(out_param_value);

  uint16_t key;
  if (!reader.ReadU16BigEndian(key)) {
    return false;
  }
  if (last_key.has_value() && last_key.value() >= key)
    return false;

  base::span<const uint8_t> value;
  if (!dns_names_util::ReadU16LengthPrefixed(reader, &value)) {
    return false;
  }

  *out_param_key = key;
  *out_param_value = base::as_string_view(value);
  return true;
}

bool ParseMandatoryKeys(std::string_view param_value,
                        std::set<uint16_t>* out_parsed) {
  DCHECK(out_parsed);

  auto reader = base::SpanReader(base::as_byte_span(param_value));

  std::set<uint16_t> mandatory_keys;
  // Do/while to require at least one key.
  do {
    uint16_t key;
    if (!reader.ReadU16BigEndian(key)) {
      return false;
    }

    // Mandatory key itself is disallowed from its list.
    if (key == dns_protocol::kHttpsServiceParamKeyMandatory)
      return false;
    // Keys required to be listed in ascending order.
    if (!mandatory_keys.empty() && key <= *mandatory_keys.rbegin())
      return false;

    CHECK(mandatory_keys.insert(key).second);
  } while (reader.remaining() > 0u);

  *out_parsed = std::move(mandatory_keys);
  return true;
}

bool ParseAlpnIds(std::string_view param_value,
                  std::vector<std::string>* out_parsed) {
  DCHECK(out_parsed);

  auto reader = base::SpanReader(base::as_byte_span(param_value));

  std::vector<std::string> alpn_ids;
  // Do/while to require at least one ID.
  do {
    base::span<const uint8_t> alpn_id;
    if (!dns_names_util::ReadU8LengthPrefixed(reader, &alpn_id)) {
      return false;
    }
    if (alpn_id.size() < 1u) {
      return false;
    }
    DCHECK_LE(alpn_id.size(), 255u);

    alpn_ids.emplace_back(base::as_string_view(alpn_id));
  } while (reader.remaining() > 0u);

  *out_parsed = std::move(alpn_ids);
  return true;
}

template <size_t ADDRESS_SIZE>
bool ParseIpAddresses(std::string_view param_value,
                      std::vector<IPAddress>* out_addresses) {
  DCHECK(out_addresses);

  auto reader = base::SpanReader(base::as_byte_span(param_value));

  std::vector<IPAddress> addresses;
  do {
    if (auto addr_bytes = reader.template Read<ADDRESS_SIZE>();
        !addr_bytes.has_value()) {
      return false;
    } else {
      addresses.emplace_back(*addr_bytes);
    }
    DCHECK(addresses.back().IsValid());
  } while (reader.remaining() > 0u);

  *out_addresses = std::move(addresses);
  return true;
}

}  // namespace

// static
std::unique_ptr<HttpsRecordRdata> HttpsRecordRdata::Parse(
    std::string_view data) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto reader = base::SpanReader(base::as_byte_span(data));
  uint16_t priority;
  CHECK(reader.ReadU16BigEndian(priority));

  if (priority == 0) {
    return AliasFormHttpsRecordRdata::Parse(data);
  }
  return ServiceFormHttpsRecordRdata::Parse(data);
}

HttpsRecordRdata::~HttpsRecordRdata() = default;

bool HttpsRecordRdata::IsEqual(const RecordRdata* other) const {
  DCHECK(other);

  if (other->Type() != kType)
    return false;

  const HttpsRecordRdata* https = static_cast<const HttpsRecordRdata*>(other);
  return IsEqual(https);
}

uint16_t HttpsRecordRdata::Type() const {
  return kType;
}

AliasFormHttpsRecordRdata* HttpsRecordRdata::AsAliasForm() {
  CHECK(IsAlias());
  return static_cast<AliasFormHttpsRecordRdata*>(this);
}

const AliasFormHttpsRecordRdata* HttpsRecordRdata::AsAliasForm() const {
  return const_cast<HttpsRecordRdata*>(this)->AsAliasForm();
}

ServiceFormHttpsRecordRdata* HttpsRecordRdata::AsServiceForm() {
  CHECK(!IsAlias());
  return static_cast<ServiceFormHttpsRecordRdata*>(this);
}

const ServiceFormHttpsRecordRdata* HttpsRecordRdata::AsServiceForm() const {
  return const_cast<HttpsRecordRdata*>(this)->AsServiceForm();
}

AliasFormHttpsRecordRdata::AliasFormHttpsRecordRdata(std::string alias_name)
    : alias_name_(std::move(alias_name)) {}

// static
std::unique_ptr<AliasFormHttpsRecordRdata> AliasFormHttpsRecordRdata::Parse(
    std::string_view data) {
  auto reader = base::SpanReader(base::as_byte_span(data));

  uint16_t priority;
  if (!reader.ReadU16BigEndian(priority)) {
    return nullptr;
  }
  if (priority != 0u) {
    return nullptr;
  }

  std::optional<std::string> alias_name =
      dns_names_util::NetworkToDottedName(reader, true /* require_complete */);
  if (!alias_name.has_value())
    return nullptr;

  // Ignore any params.
  std::optional<uint16_t> last_param_key;
  while (reader.remaining() > 0u) {
    uint16_t param_key;
    std::string_view param_value;
    if (!ReadNextServiceParam(last_param_key, reader, &param_key, &param_value))
      return nullptr;
    last_param_key = param_key;
  }

  return std::make_unique<AliasFormHttpsRecordRdata>(
      std::move(alias_name).value());
}

bool AliasFormHttpsRecordRdata::IsEqual(const HttpsRecordRdata* other) const {
  DCHECK(other);

  if (!other->IsAlias())
    return false;

  const AliasFormHttpsRecordRdata* alias = other->AsAliasForm();
  return alias_name_ == alias->alias_name_;
}

bool AliasFormHttpsRecordRdata::IsAlias() const {
  return true;
}

// static
constexpr uint16_t ServiceFormHttpsRecordRdata::kSupportedKeys[];

ServiceFormHttpsRecordRdata::ServiceFormHttpsRecordRdata(
    HttpsRecordPriority priority,
    std::string service_name,
    std::set<uint16_t> mandatory_keys,
    std::vector<std::string> alpn_ids,
    bool default_alpn,
    std::optional<uint16_t> port,
    std::vector<IPAddress> ipv4_hint,
    std::string ech_config,
    std::vector<IPAddress> ipv6_hint,
    std::map<uint16_t, std::string> unparsed_params)
    : priority_(priority),
      service_name_(std::move(service_name)),
      mandatory_keys_(std::move(mandatory_keys)),
      alpn_ids_(std::move(alpn_ids)),
      default_alpn_(default_alpn),
      port_(port),
      ipv4_hint_(std::move(ipv4_hint)),
      ech_config_(std::move(ech_config)),
      ipv6_hint_(std::move(ipv6_hint)),
      unparsed_params_(std::move(unparsed_params)) {
  DCHECK_NE(priority_, 0);
  DCHECK(!base::Contains(mandatory_keys_,
                         dns_protocol::kHttpsServiceParamKeyMandatory));

#if DCHECK_IS_ON()
  for (const IPAddress& address : ipv4_hint_) {
    DCHECK(address.IsIPv4());
  }
  for (const IPAddress& address : ipv6_hint_) {
    DCHECK(address.IsIPv6());
  }
  for (const auto& unparsed_param : unparsed_params_) {
    DCHECK(!IsSupportedKey(unparsed_param.first));
  }
#endif  // DCHECK_IS_ON()
}

ServiceFormHttpsRecordRdata::~ServiceFormHttpsRecordRdata() = default;

bool ServiceFormHttpsRecordRdata::IsEqual(const HttpsRecordRdata* other) const {
  DCHECK(other);

  if (other->IsAlias())
    return false;

  const ServiceFormHttpsRecordRdata* service = other->AsServiceForm();
  return priority_ == service->priority_ &&
         service_name_ == service->service_name_ &&
         mandatory_keys_ == service->mandatory_keys_ &&
         alpn_ids_ == service->alpn_ids_ &&
         default_alpn_ == service->default_alpn_ && port_ == service->port_ &&
         ipv4_hint_ == service->ipv4_hint_ &&
         ech_config_ == service->ech_config_ &&
         ipv6_hint_ == service->ipv6_hint_;
}

bool ServiceFormHttpsRecordRdata::IsAlias() const {
  return false;
}

// static
std::unique_ptr<ServiceFormHttpsRecordRdata> ServiceFormHttpsRecordRdata::Parse(
    std::string_view data) {
  auto reader = base::SpanReader(base::as_byte_span(data));

  uint16_t priority;
  if (!reader.ReadU16BigEndian(priority)) {
    return nullptr;
  }
  if (priority == 0u) {
    return nullptr;
  }

  std::optional<std::string> service_name =
      dns_names_util::NetworkToDottedName(reader, true /* require_complete */);
  if (!service_name.has_value())
    return nullptr;

  if (reader.remaining() == 0u) {
    return std::make_unique<ServiceFormHttpsRecordRdata>(
        HttpsRecordPriority{priority}, std::move(service_name).value(),
        std::set<uint16_t>() /* mandatory_keys */,
        std::vector<std::string>() /* alpn_ids */, true /* default_alpn */,
        std::nullopt /* port */, std::vector<IPAddress>() /* ipv4_hint */,
        std::string() /* ech_config */,
        std::vector<IPAddress>() /* ipv6_hint */,
        std::map<uint16_t, std::string>() /* unparsed_params */);
  }

  uint16_t param_key = 0;
  std::string_view param_value;
  if (!ReadNextServiceParam(std::nullopt /* last_key */, reader, &param_key,
                            &param_value)) {
    return nullptr;
  }

  // Assume keys less than Mandatory are not possible.
  DCHECK_GE(param_key, dns_protocol::kHttpsServiceParamKeyMandatory);

  std::set<uint16_t> mandatory_keys;
  if (param_key == dns_protocol::kHttpsServiceParamKeyMandatory) {
    DCHECK(IsSupportedKey(param_key));
    if (!ParseMandatoryKeys(param_value, &mandatory_keys))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::vector<std::string> alpn_ids;
  if (param_key == dns_protocol::kHttpsServiceParamKeyAlpn) {
    DCHECK(IsSupportedKey(param_key));
    if (!ParseAlpnIds(param_value, &alpn_ids))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  bool default_alpn = true;
  if (param_key == dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn) {
    DCHECK(IsSupportedKey(param_key));
    if (!param_value.empty())
      return nullptr;
    default_alpn = false;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::optional<uint16_t> port;
  if (param_key == dns_protocol::kHttpsServiceParamKeyPort) {
    DCHECK(IsSupportedKey(param_key));
    if (param_value.size() != 2)
      return nullptr;
    uint16_t port_val =
        base::U16FromBigEndian(base::as_byte_span(param_value).first<2>());
    port = port_val;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::vector<IPAddress> ipv4_hint;
  if (param_key == dns_protocol::kHttpsServiceParamKeyIpv4Hint) {
    DCHECK(IsSupportedKey(param_key));
    if (!ParseIpAddresses<IPAddress::kIPv4AddressSize>(param_value, &ipv4_hint))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::string ech_config;
  if (param_key == dns_protocol::kHttpsServiceParamKeyEchConfig) {
    DCHECK(IsSupportedKey(param_key));
    ech_config = std::string(param_value.data(), param_value.size());
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::vector<IPAddress> ipv6_hint;
  if (param_key == dns_protocol::kHttpsServiceParamKeyIpv6Hint) {
    DCHECK(IsSupportedKey(param_key));
    if (!ParseIpAddresses<IPAddress::kIPv6AddressSize>(param_value, &ipv6_hint))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  // Note that if parsing has already reached the end of the rdata, `param_key`
  // is still set for whatever param was read last.
  std::map<uint16_t, std::string> unparsed_params;
  if (param_key > dns_protocol::kHttpsServiceParamKeyIpv6Hint) {
    for (;;) {
      DCHECK(!IsSupportedKey(param_key));
      CHECK(unparsed_params
                .emplace(param_key, static_cast<std::string>(param_value))
                .second);
      if (reader.remaining() == 0)
        break;
      if (!ReadNextServiceParam(param_key, reader, &param_key, &param_value))
        return nullptr;
    }
  }

  return std::make_unique<ServiceFormHttpsRecordRdata>(
      HttpsRecordPriority{priority}, std::move(service_name).value(),
      std::move(mandatory_keys), std::move(alpn_ids), default_alpn, port,
      std::move(ipv4_hint), std::move(ech_config), std::move(ipv6_hint),
      std::move(unparsed_params));
}

bool ServiceFormHttpsRecordRdata::IsCompatible() const {
  std::set<uint16_t> supported_keys(std::begin(kSupportedKeys),
                                    std::end(kSupportedKeys));

  for (uint16_t mandatory_key : mandatory_keys_) {
    DCHECK_NE(mandatory_key, dns_protocol::kHttpsServiceParamKeyMandatory);

    if (!base::Contains(supported_keys, mandatory_key)) {
      return false;
    }
  }

#if DCHECK_IS_ON()
  for (const auto& unparsed_param : unparsed_params_) {
    DCHECK(!base::Contains(mandatory_keys_, unparsed_param.first));
  }
#endif  // DCHECK_IS_ON()

  return true;
}

// static
bool ServiceFormHttpsRecordRdata::IsSupportedKey(uint16_t key) {
#if DCHECK_IS_ON()
  return base::Contains(kSupportedKeys, key);
#else
  // Only intended for DCHECKs.
  base::ImmediateCrash();
#endif  // DCHECK_IS_ON()
}

}  // namespace net
