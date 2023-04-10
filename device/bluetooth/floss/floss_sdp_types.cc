// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_sdp_types.h"

namespace floss {

BtSdpHeaderOverlay::BtSdpHeaderOverlay() = default;
BtSdpHeaderOverlay::BtSdpHeaderOverlay(const BtSdpHeaderOverlay& other) =
    default;
BtSdpHeaderOverlay::~BtSdpHeaderOverlay() = default;

constexpr char kTypeKey[] = "type";
constexpr char kVariantValueKey[] = "0";

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpType* sdp_type) {
  uint32_t raw_type = 0;
  if (!FlossDBusClient::ReadDBusParam(reader, &raw_type)) {
    return false;
  }

  *sdp_type = static_cast<BtSdpType>(raw_type);
  return true;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpHeaderOverlay* header_overlay) {
  static StructReader<BtSdpHeaderOverlay> struct_reader(
      {{"sdp_type", CreateFieldReader(&BtSdpHeaderOverlay::sdp_type)},
       {"uuid", CreateFieldReader(&BtSdpHeaderOverlay::uuid)},
       {"service_name_length",
        CreateFieldReader(&BtSdpHeaderOverlay::service_name_length)},
       {"service_name", CreateFieldReader(&BtSdpHeaderOverlay::service_name)},
       {"rfcomm_channel_number",
        CreateFieldReader(&BtSdpHeaderOverlay::rfcomm_channel_number)},
       {"l2cap_psm", CreateFieldReader(&BtSdpHeaderOverlay::l2cap_psm)},
       {"profile_version",
        CreateFieldReader(&BtSdpHeaderOverlay::profile_version)},
       {"user1_len", CreateFieldReader(&BtSdpHeaderOverlay::user1_len)},
       {"user1_data", CreateFieldReader(&BtSdpHeaderOverlay::user1_data)},
       {"user2_len", CreateFieldReader(&BtSdpHeaderOverlay::user2_len)},
       {"user2_data", CreateFieldReader(&BtSdpHeaderOverlay::user2_data)}});
  return struct_reader.ReadDBusParam(reader, header_overlay);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpHeaderOverlay*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpHeaderOverlay"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpMasRecord* record) {
  static StructReader<BtSdpMasRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpMasRecord::hdr)},
       {"mas_instance_id", CreateFieldReader(&BtSdpMasRecord::mas_instance_id)},
       {"supported_features",
        CreateFieldReader(&BtSdpMasRecord::supported_features)},
       {"supported_message_types",
        CreateFieldReader(&BtSdpMasRecord::supported_message_types)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpMasRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpMasRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpMnsRecord* record) {
  static StructReader<BtSdpMnsRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpMnsRecord::hdr)},
       {"supported_features",
        CreateFieldReader(&BtSdpMnsRecord::supported_features)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpMnsRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpMnsRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpPseRecord* record) {
  static StructReader<BtSdpPseRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpPseRecord::hdr)},
       {"supported_features",
        CreateFieldReader(&BtSdpPseRecord::supported_features)},
       {"supported_repositories",
        CreateFieldReader(&BtSdpPseRecord::supported_repositories)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpPseRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpPseRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpPceRecord* record) {
  static StructReader<BtSdpPceRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpPceRecord::hdr)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpPceRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpPceRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpOpsRecord* record) {
  static StructReader<BtSdpOpsRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpOpsRecord::hdr)},
       {"supported_formats_list_len",
        CreateFieldReader(&BtSdpOpsRecord::supported_formats_list_len)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpOpsRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpOpsRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpSapRecord* record) {
  static StructReader<BtSdpSapRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpSapRecord::hdr)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpSapRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpSapRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpDipRecord* record) {
  static StructReader<BtSdpDipRecord> struct_reader(
      {{"hdr", CreateFieldReader(&BtSdpDipRecord::hdr)},
       {"spec_id", CreateFieldReader(&BtSdpDipRecord::spec_id)},
       {"vendor", CreateFieldReader(&BtSdpDipRecord::vendor)},
       {"vendor_id_source",
        CreateFieldReader(&BtSdpDipRecord::vendor_id_source)},
       {"product", CreateFieldReader(&BtSdpDipRecord::product)},
       {"version", CreateFieldReader(&BtSdpDipRecord::version)},
       {"primary_record", CreateFieldReader(&BtSdpDipRecord::primary_record)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpDipRecord*) {
  static DBusTypeInfo info{"a{sv}", "BtSdpDipRecord"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpRecord* record) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader)) {
    return false;
  }

  // We have no guarantee of the order of map entries, and we can't access map
  // fields arbitrarily from the MessageReader. We don't know the number or
  // types of arguments until we find the "type" key so first we will parse out
  // all of the entries into a map with the keys resolved.
  std::unordered_map<std::string, std::unique_ptr<dbus::MessageReader>>
      unparsed_args;
  while (array_reader.HasMoreData()) {
    std::unique_ptr<dbus::MessageReader> entry_reader =
        std::make_unique<dbus::MessageReader>(nullptr);
    if (!array_reader.PopDictEntry(entry_reader.get())) {
      return false;
    }

    std::string key;
    if (!entry_reader->PopString(&key)) {
      return false;
    }

    unparsed_args[key] = std::move(entry_reader);
  }

  if (!unparsed_args.contains(kTypeKey)) {
    LOG(ERROR) << "BtSdpRecord did not contain type identifier";
    return false;
  }

  if (!unparsed_args.contains(kVariantValueKey)) {
    LOG(ERROR) << "BtSdpRecord did not contain argument #0";
    return false;
  }

  dbus::MessageReader sdp_type_variant_reader = dbus::MessageReader(nullptr);
  BtSdpType sdp_type;
  if (!unparsed_args[kTypeKey]->PopVariant(&sdp_type_variant_reader) ||
      !ReadDBusParam(&sdp_type_variant_reader, &sdp_type)) {
    return false;
  }

  dbus::MessageReader variant_reader = dbus::MessageReader(nullptr);
  switch (sdp_type) {
    case BtSdpType::kRaw: {
      BtSdpHeaderOverlay header{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &header)) {
        return false;
      }
      *record = header;
      break;
    }
    case BtSdpType::kMapMas: {
      BtSdpMasRecord mas_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &mas_record)) {
        return false;
      }
      *record = mas_record;
      break;
    }
    case BtSdpType::kMapMns: {
      BtSdpMnsRecord mns_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &mns_record)) {
        return false;
      }
      *record = mns_record;
      break;
    }
    case BtSdpType::kPbapPse: {
      BtSdpPseRecord pse_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &pse_record)) {
        return false;
      }
      *record = pse_record;
      break;
    }
    case BtSdpType::kPbapPce: {
      BtSdpPceRecord pce_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &pce_record)) {
        return false;
      }
      *record = pce_record;
      break;
    }
    case BtSdpType::kOppServer: {
      BtSdpOpsRecord ops_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &ops_record)) {
        return false;
      }
      *record = ops_record;
      break;
    }
    case BtSdpType::kSapServer: {
      BtSdpSapRecord sap_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &sap_record)) {
        return false;
      }
      *record = sap_record;
      break;
    }
    case BtSdpType::kDip: {
      BtSdpDipRecord dip_record{};
      if (!unparsed_args[kVariantValueKey]->PopVariant(&variant_reader) ||
          !ReadDBusParam(&variant_reader, &dip_record)) {
        return false;
      }
      *record = dip_record;
      break;
    }
    default:
      return false;
  }
  return true;
}

}  // namespace floss
