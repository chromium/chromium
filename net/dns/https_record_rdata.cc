// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

bool ReadNextServiceParam(base::Optional<uint16_t> last_key,
                          base::BigEndianReader& reader,
                          uint16_t* out_param_key,
                          base::StringPiece* out_param_value) {
  DCHECK(out_param_key);
  DCHECK(out_param_value);

  uint16_t key;
  if (!reader.ReadU16(&key))
    return false;
  if (last_key.has_value() && last_key.value() >= key)
    return false;

  base::StringPiece value;
  if (!reader.ReadU16LengthPrefixed(&value))
    return false;

  *out_param_key = key;
  *out_param_value = value;
  return true;
}

bool ParseAlpnIds(base::StringPiece param_value,
                  std::vector<std::string>* out_parsed) {
  DCHECK(out_parsed);

  base::BigEndianReader reader(param_value.data(), param_value.size());

  std::vector<std::string> alpn_ids;
  // Do/while to require at least one ID.
  do {
    base::StringPiece alpn_id;
    if (!reader.ReadU8LengthPrefixed(&alpn_id))
      return false;
    if (alpn_id.size() < 1)
      return false;
    DCHECK_LE(alpn_id.size(), 255u);

    alpn_ids.emplace_back(alpn_id.data(), alpn_id.size());
  } while (reader.remaining() > 0);

  *out_parsed = std::move(alpn_ids);
  return true;
}

template <size_t ADDRESS_SIZE>
bool ParseIpAddresses(base::StringPiece param_value,
                      std::vector<IPAddress>* out_addresses) {
  DCHECK(out_addresses);

  base::BigEndianReader reader(param_value.data(), param_value.size());

  std::vector<IPAddress> addresses;
  uint8_t addr_bytes[ADDRESS_SIZE];
  do {
    if (!reader.ReadBytes(addr_bytes, ADDRESS_SIZE))
      return false;
    addresses.emplace_back(addr_bytes);
    DCHECK(addresses.back().IsValid());
  } while (reader.remaining() > 0);

  *out_addresses = std::move(addresses);
  return true;
}

}  // namespace

// static
std::unique_ptr<HttpsRecordRdata> HttpsRecordRdata::Parse(
    base::StringPiece data) {
  if (!HasValidSize(data, kType))
    return nullptr;

  base::BigEndianReader reader(data.data(), data.size());
  uint16_t priority;
  CHECK(reader.ReadU16(&priority));

  if (priority == 0) {
    return AliasFormHttpsRecordRdata::Parse(data);
  } else {
    return ServiceFormHttpsRecordRdata::Parse(data);
  }
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
    : alias_name_(std::move(alias_name)) {
  DCHECK(!alias_name_.empty());
}

// static
std::unique_ptr<AliasFormHttpsRecordRdata> AliasFormHttpsRecordRdata::Parse(
    base::StringPiece data) {
  base::BigEndianReader reader(data.data(), data.size());

  uint16_t priority;
  if (!reader.ReadU16(&priority))
    return nullptr;
  if (priority != 0)
    return nullptr;

  base::Optional<std::string> alias_name =
      DnsDomainToString(reader, true /* require_complete */);
  if (!alias_name.has_value() || alias_name.value().empty())
    return nullptr;

  if (reader.remaining() != 0)
    return nullptr;

  return std::make_unique<AliasFormHttpsRecordRdata>(
      std::move(alias_name).value());
}

bool AliasFormHttpsRecordRdata::IsEqual(const HttpsRecordRdata* other) const {
  DCHECK(other);

  if (!other->IsAlias())
    return false;

  const AliasFormHttpsRecordRdata* alias =
      static_cast<const AliasFormHttpsRecordRdata*>(other);
  return alias_name_ == alias->alias_name_;
}

bool AliasFormHttpsRecordRdata::IsAlias() const {
  return true;
}

ServiceFormHttpsRecordRdata::ServiceFormHttpsRecordRdata(
    uint16_t priority,
    std::string service_name,
    std::vector<std::string> alpn_ids,
    bool default_alpn,
    base::Optional<uint16_t> port,
    std::vector<IPAddress> ipv4_hint,
    std::string ech_config,
    std::vector<IPAddress> ipv6_hint,
    std::map<uint16_t, std::string> unparsed_params)
    : priority_(priority),
      service_name_(std::move(service_name)),
      alpn_ids_(std::move(alpn_ids)),
      default_alpn_(default_alpn),
      port_(port),
      ipv4_hint_(std::move(ipv4_hint)),
      ech_config_(std::move(ech_config)),
      ipv6_hint_(std::move(ipv6_hint)),
      unparsed_params_(std::move(unparsed_params)) {
  DCHECK_NE(priority_, 0);

#if DCHECK_IS_ON()
  for (const IPAddress& address : ipv4_hint) {
    DCHECK(address.IsIPv4());
  }
  for (const IPAddress& address : ipv6_hint) {
    DCHECK(address.IsIPv6());
  }
#endif  // DCHECK_IS_ON()
}

ServiceFormHttpsRecordRdata::~ServiceFormHttpsRecordRdata() = default;

bool ServiceFormHttpsRecordRdata::IsEqual(const HttpsRecordRdata* other) const {
  DCHECK(other);

  if (other->IsAlias())
    return false;

  const ServiceFormHttpsRecordRdata* service =
      static_cast<const ServiceFormHttpsRecordRdata*>(other);
  return priority_ == service->priority_ &&
         service_name_ == service->service_name_ &&
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
    base::StringPiece data) {
  base::BigEndianReader reader(data.data(), data.size());

  uint16_t priority;
  if (!reader.ReadU16(&priority))
    return nullptr;
  if (priority == 0)
    return nullptr;

  base::Optional<std::string> service_name =
      DnsDomainToString(reader, true /* require_complete */);
  if (!service_name.has_value())
    return nullptr;

  if (reader.remaining() == 0) {
    return std::make_unique<ServiceFormHttpsRecordRdata>(
        priority, std::move(service_name).value(),
        std::vector<std::string>() /* alpn_ids */, true /* default_alpn */,
        base::nullopt /* port */, std::vector<IPAddress>() /* ipv4_hint */,
        std::string() /* ech_config */,
        std::vector<IPAddress>() /* ipv6_hint */,
        std::map<uint16_t, std::string>() /* unparsed_params */);
  }

  uint16_t param_key = 0;
  base::StringPiece param_value;
  if (!ReadNextServiceParam(base::nullopt /* last_key */, reader, &param_key,
                            &param_value))
    return nullptr;

  std::map<uint16_t, std::string> unparsed_params;
  while (param_key < dns_protocol::kHttpsServiceParamKeyAlpn) {
    CHECK(unparsed_params
              .emplace(param_key, static_cast<std::string>(param_value))
              .second);
    if (reader.remaining() == 0)
      break;
    if (!ReadNextServiceParam(param_key, reader, &param_key, &param_value))
      return nullptr;
  }

  std::vector<std::string> alpn_ids;
  if (param_key == dns_protocol::kHttpsServiceParamKeyAlpn) {
    if (!ParseAlpnIds(param_value, &alpn_ids))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  bool default_alpn = true;
  if (param_key == dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn) {
    if (!param_value.empty())
      return nullptr;
    default_alpn = false;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  base::Optional<uint16_t> port;
  if (param_key == dns_protocol::kHttpsServiceParamKeyPort) {
    if (param_value.size() != 2)
      return nullptr;
    uint16_t port_val;
    base::ReadBigEndian(param_value.data(), &port_val);
    port = port_val;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::vector<IPAddress> ipv4_hint;
  if (param_key == dns_protocol::kHttpsServiceParamKeyIpv4Hint) {
    if (!ParseIpAddresses<IPAddress::kIPv4AddressSize>(param_value, &ipv4_hint))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::string ech_config;
  if (param_key == dns_protocol::kHttpsServiceParamKeyEchConfig) {
    ech_config = std::string(param_value.data(), param_value.size());
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  std::vector<IPAddress> ipv6_hint;
  if (param_key == dns_protocol::kHttpsServiceParamKeyIpv6Hint) {
    if (!ParseIpAddresses<IPAddress::kIPv6AddressSize>(param_value, &ipv6_hint))
      return nullptr;
    if (reader.remaining() > 0 &&
        !ReadNextServiceParam(param_key, reader, &param_key, &param_value)) {
      return nullptr;
    }
  }

  // Note that if parsing has already reached the end of the rdata, `param_key`
  // is still set for whatever param was read last.
  if (param_key > dns_protocol::kHttpsServiceParamKeyIpv6Hint) {
    for (;;) {
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
      priority, std::move(service_name).value(), std::move(alpn_ids),
      default_alpn, port, std::move(ipv4_hint), std::move(ech_config),
      std::move(ipv6_hint), std::move(unparsed_params));
}

}  // namespace net
