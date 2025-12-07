// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

constexpr auto kSupportedKeys = base::MakeFixedFlatSet<uint16_t>(
    base::sorted_unique,
    {
        dns_protocol::kHttpsServiceParamKeyMandatory,
        dns_protocol::kHttpsServiceParamKeyAlpn,
        dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn,
        dns_protocol::kHttpsServiceParamKeyPort,
        dns_protocol::kHttpsServiceParamKeyIpv4Hint,
        dns_protocol::kHttpsServiceParamKeyEchConfig,
        dns_protocol::kHttpsServiceParamKeyIpv6Hint,
        dns_protocol::kHttpsServiceParamKeyTrustAnchorIDs,
    });

bool ReadNextServiceParam(std::optional<uint16_t> last_key,
                          base::SpanReader<const uint8_t>& reader,
                          uint16_t* out_param_key,
                          base::span<const uint8_t>* out_param_value) {
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
  *out_param_value = value;
  return true;
}

bool ParseMandatoryKeys(base::span<const uint8_t> param_value,
                        base::flat_set<uint16_t>* out_parsed) {
  DCHECK(out_parsed);

  auto reader = base::SpanReader(param_value);

  std::vector<uint16_t> mandatory_keys;
  // Do/while to require at least one key.
  do {
    uint16_t key;
    if (!reader.ReadU16BigEndian(key)) {
      return false;
    }

    // Mandatory key itself is disallowed from its list.
    if (key == dns_protocol::kHttpsServiceParamKeyMandatory) {
      return false;
    }
    // Keys required to be listed in ascending order.
    if (!mandatory_keys.empty() && key <= mandatory_keys.back()) {
      return false;
    }

    mandatory_keys.push_back(key);
  } while (reader.remaining() > 0u);

  // The parsing process ensures the keys are already in sorted order.
  *out_parsed =
      base::flat_set<uint16_t>(base::sorted_unique, std::move(mandatory_keys));
  return true;
}

bool ParseAlpnIds(base::span<const uint8_t> param_value,
                  std::vector<std::string>* out_parsed) {
  DCHECK(out_parsed);

  auto reader = base::SpanReader(param_value);

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
bool ParseIpAddresses(base::span<const uint8_t> param_value,
                      std::vector<IPAddress>* out_addresses) {
  DCHECK(out_addresses);

  auto reader = base::SpanReader(param_value);

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

bool ParseTrustAnchorIDs(
    base::span<const uint8_t> param_value,
    std::vector<std::vector<uint8_t>>* out_trust_anchor_ids) {
  DCHECK(out_trust_anchor_ids);

  auto reader = base::SpanReader(param_value);
  std::vector<std::vector<uint8_t>> trust_anchor_ids;
  do {
    base::span<const uint8_t> trust_anchor_id;
    if (!dns_names_util::ReadU8LengthPrefixed(reader, &trust_anchor_id)) {
      return false;
    }
    if (trust_anchor_id.size() < 1u) {
      return false;
    }
    DCHECK_LE(trust_anchor_id.size(), 255u);

    trust_anchor_ids.emplace_back(base::ToVector(trust_anchor_id));
  } while (reader.remaining() > 0u);

  *out_trust_anchor_ids = std::move(trust_anchor_ids);
  return true;
}

}  // namespace

// static
std::unique_ptr<HttpsRecordRdata> HttpsRecordRdata::Parse(
    base::span<const uint8_t> data) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto reader = base::SpanReader(data);
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
    base::span<const uint8_t> data) {
  auto reader = base::SpanReader(data);

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
    base::span<const uint8_t> param_value;
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

ServiceFormHttpsRecordRdata::ServiceFormHttpsRecordRdata(
    HttpsRecordPriority priority,
    std::string service_name,
    base::flat_set<uint16_t> mandatory_keys,
    std::vector<std::string> alpn_ids,
    bool default_alpn,
    std::optional<uint16_t> port,
    std::vector<IPAddress> ipv4_hint,
    base::span<const uint8_t> ech_config,
    std::vector<IPAddress> ipv6_hint,
    std::vector<std::vector<uint8_t>> trust_anchor_ids)
    : priority_(priority),
      service_name_(std::move(service_name)),
      mandatory_keys_(std::move(mandatory_keys)),
      alpn_ids_(std::move(alpn_ids)),
      default_alpn_(default_alpn),
      port_(port),
      ipv4_hint_(std::move(ipv4_hint)),
      ech_config_(ech_config.begin(), ech_config.end()),
      ipv6_hint_(std::move(ipv6_hint)),
      trust_anchor_ids_(std::move(trust_anchor_ids)) {
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
         ipv6_hint_ == service->ipv6_hint_ &&
         trust_anchor_ids_ == service->trust_anchor_ids_;
}

bool ServiceFormHttpsRecordRdata::IsAlias() const {
  return false;
}

// static
std::unique_ptr<ServiceFormHttpsRecordRdata> ServiceFormHttpsRecordRdata::Parse(
    base::span<const uint8_t> data) {
  auto reader = base::SpanReader(data);

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
        base::flat_set<uint16_t>() /* mandatory_keys */,
        std::vector<std::string>() /* alpn_ids */, true /* default_alpn */,
        std::nullopt /* port */, std::vector<IPAddress>() /* ipv4_hint */,
        std::vector<uint8_t>() /* ech_config */,
        std::vector<IPAddress>() /* ipv6_hint */,
        std::vector<std::vector<uint8_t>>() /* trust_anchor_ids */);
  }

  uint16_t param_key = 0;
  base::span<const uint8_t> param_value;
  if (!ReadNextServiceParam(std::nullopt /* last_key */, reader, &param_key,
                            &param_value)) {
    return nullptr;
  }

  // Assume keys less than Mandatory are not possible.
  DCHECK_GE(param_key, dns_protocol::kHttpsServiceParamKeyMandatory);

  base::flat_set<uint16_t> mandatory_keys;
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

  std::vector<uint8_t> ech_config;
  if (param_key == dns_protocol::kHttpsServiceParamKeyEchConfig) {
    DCHECK(IsSupportedKey(param_key));
    ech_config = std::vector<uint8_t>(param_value.begin(), param_value.end());
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

  // Above kHttpsServiceParamKeyIpv6Hint, supported keys are no longer
  // contiguous. Read the remainder of the record data (if there is any),
  // processing the non-contiguous supported key if present and otherwise just
  // checking that the unsupported keys are well-formed.
  std::vector<std::vector<uint8_t>> trust_anchor_ids;
  while (reader.remaining() > 0 &&
         param_key < dns_protocol::kHttpsServiceParamKeyTrustAnchorIDs) {
    DCHECK(!IsSupportedKey(param_key));
    if (!ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  if (param_key == dns_protocol::kHttpsServiceParamKeyTrustAnchorIDs) {
    DCHECK(IsSupportedKey(param_key));
    if (!ParseTrustAnchorIDs(param_value, &trust_anchor_ids)) {
      return nullptr;
    }
  }

  for (;;) {
    if (reader.remaining() == 0) {
      break;
    }
    if (!ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
    DCHECK(!IsSupportedKey(param_key));
  }

  return std::make_unique<ServiceFormHttpsRecordRdata>(
      HttpsRecordPriority{priority}, std::move(service_name).value(),
      std::move(mandatory_keys), std::move(alpn_ids), default_alpn, port,
      std::move(ipv4_hint), ech_config, std::move(ipv6_hint),
      std::move(trust_anchor_ids));
}

bool ServiceFormHttpsRecordRdata::IsCompatible() const {
  for (uint16_t mandatory_key : mandatory_keys_) {
    DCHECK_NE(mandatory_key, dns_protocol::kHttpsServiceParamKeyMandatory);
    if (!base::Contains(kSupportedKeys, mandatory_key)) {
      return false;
    }
  }
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
