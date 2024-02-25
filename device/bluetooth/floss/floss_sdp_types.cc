// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_sdp_types.h"

#include "base/containers/contains.h"

namespace floss {

BtSdpHeaderOverlay::BtSdpHeaderOverlay() = default;
BtSdpHeaderOverlay::BtSdpHeaderOverlay(const BtSdpHeaderOverlay& other) =
    default;
BtSdpHeaderOverlay::~BtSdpHeaderOverlay() = default;

constexpr char kTypeKey[] = "type";
constexpr char kVariantValueKey[] = "0";

constexpr char kSdpHeaderOverlayPropSdpType[] = "sdp_type";
constexpr char kSdpHeaderOverlayPropUuid[] = "uuid";
constexpr char kSdpHeaderOverlayPropServiceNameLength[] = "service_name_length";
constexpr char kSdpHeaderOverlayPropServiceName[] = "service_name";
constexpr char kSdpHeaderOverlayPropRfcommChannelNumber[] =
    "rfcomm_channel_number";
constexpr char kSdpHeaderOverlayPropL2capPsm[] = "l2cap_psm";
constexpr char kSdpHeaderOverlayPropProfileVersion[] = "profile_version";
constexpr char kSdpHeaderOverlayPropUser1Len[] = "user1_len";
constexpr char kSdpHeaderOverlayPropUser1Data[] = "user1_data";
constexpr char kSdpHeaderOverlayPropUser2Len[] = "user2_len";
constexpr char kSdpHeaderOverlayPropUser2Data[] = "user2_data";

// All record types share a "hdr" field.
constexpr char kSdpRecordPropHdr[] = "hdr";

constexpr char kSdpMasRecordPropMasInstanceId[] = "mas_instance_id";
constexpr char kSdpMasRecordPropSupportedFeatures[] = "supported_features";
constexpr char kSdpMasRecordPropSupportedMessageTypes[] =
    "supported_message_types";

constexpr char kSdpMnsRecordPropSupportedFeatures[] = "supported_features";

constexpr char kSdpPseRecordPropSupportedFeatures[] = "supported_features";
constexpr char kSdpPseRecordPropSupportedRepositories[] =
    "supported_repositories";

constexpr char kSdpOpsRecordPropSupportedFormatsListLen[] =
    "supported_formats_list_len";

constexpr char kSdpDipRecordPropSpecId[] = "spec_id";
constexpr char kSdpDipRecordPropVendor[] = "vendor";
constexpr char kSdpDipRecordPropVendorIdSource[] = "vendor_id_source";
constexpr char kSdpDipRecordPropProduct[] = "product";
constexpr char kSdpDipRecordPropVersion[] = "version";
constexpr char kSdpDipRecordPropPrimaryRecord[] = "primary_record";

std::optional<floss::BtSdpHeaderOverlay> GetHeaderOverlayFromSdpRecord(
    const floss::BtSdpRecord& record) {
  if (absl::holds_alternative<floss::BtSdpHeaderOverlay>(record)) {
    return absl::get<floss::BtSdpHeaderOverlay>(record);
  } else if (absl::holds_alternative<floss::BtSdpMasRecord>(record)) {
    return absl::get<floss::BtSdpMasRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpMnsRecord>(record)) {
    return absl::get<floss::BtSdpMnsRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpPseRecord>(record)) {
    return absl::get<floss::BtSdpPseRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpPceRecord>(record)) {
    return absl::get<floss::BtSdpPceRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpOpsRecord>(record)) {
    return absl::get<floss::BtSdpOpsRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpSapRecord>(record)) {
    return absl::get<floss::BtSdpSapRecord>(record).hdr;
  } else if (absl::holds_alternative<floss::BtSdpDipRecord>(record)) {
    return absl::get<floss::BtSdpDipRecord>(record).hdr;
  } else {
    return std::nullopt;
  }
}

std::optional<device::BluetoothUUID> GetUUIDFromSdpRecord(
    const floss::BtSdpRecord& record) {
  std::optional<floss::BtSdpHeaderOverlay> header =
      GetHeaderOverlayFromSdpRecord(record);
  if (!header.has_value()) {
    return std::nullopt;
  }
  return header->uuid;
}

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
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpType& sdp_type) {
  writer->AppendUint32(static_cast<uint32_t>(sdp_type));
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BtSdpType*) {
  static DBusTypeInfo info{"u", "BtSdpType"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BtSdpHeaderOverlay* header_overlay) {
  static StructReader<BtSdpHeaderOverlay> struct_reader(
      {{kSdpHeaderOverlayPropSdpType,
        CreateFieldReader(&BtSdpHeaderOverlay::sdp_type)},
       {kSdpHeaderOverlayPropUuid,
        CreateFieldReader(&BtSdpHeaderOverlay::uuid)},
       {kSdpHeaderOverlayPropServiceNameLength,
        CreateFieldReader(&BtSdpHeaderOverlay::service_name_length)},
       {kSdpHeaderOverlayPropServiceName,
        CreateFieldReader(&BtSdpHeaderOverlay::service_name)},
       {kSdpHeaderOverlayPropRfcommChannelNumber,
        CreateFieldReader(&BtSdpHeaderOverlay::rfcomm_channel_number)},
       {kSdpHeaderOverlayPropL2capPsm,
        CreateFieldReader(&BtSdpHeaderOverlay::l2cap_psm)},
       {kSdpHeaderOverlayPropProfileVersion,
        CreateFieldReader(&BtSdpHeaderOverlay::profile_version)},
       {kSdpHeaderOverlayPropUser1Len,
        CreateFieldReader(&BtSdpHeaderOverlay::user1_len)},
       {kSdpHeaderOverlayPropUser1Data,
        CreateFieldReader(&BtSdpHeaderOverlay::user1_data)},
       {kSdpHeaderOverlayPropUser2Len,
        CreateFieldReader(&BtSdpHeaderOverlay::user2_len)},
       {kSdpHeaderOverlayPropUser2Data,
        CreateFieldReader(&BtSdpHeaderOverlay::user2_data)}});
  return struct_reader.ReadDBusParam(reader, header_overlay);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpHeaderOverlay& header) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropSdpType, header.sdp_type);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropUuid, header.uuid);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropServiceNameLength,
                 header.service_name_length);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropServiceName,
                 header.service_name);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropRfcommChannelNumber,
                 header.rfcomm_channel_number);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropL2capPsm,
                 header.l2cap_psm);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropProfileVersion,
                 header.profile_version);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropUser1Len,
                 header.user1_len);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropUser1Data,
                 header.user1_data);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropUser2Len,
                 header.user2_len);
  WriteDictEntry(&array_writer, kSdpHeaderOverlayPropUser2Data,
                 header.user2_data);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpMasRecord::hdr)},
       {kSdpMasRecordPropMasInstanceId,
        CreateFieldReader(&BtSdpMasRecord::mas_instance_id)},
       {kSdpMasRecordPropSupportedFeatures,
        CreateFieldReader(&BtSdpMasRecord::supported_features)},
       {kSdpMasRecordPropSupportedMessageTypes,
        CreateFieldReader(&BtSdpMasRecord::supported_message_types)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpMasRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);
  WriteDictEntry(&array_writer, kSdpMasRecordPropMasInstanceId,
                 record.mas_instance_id);
  WriteDictEntry(&array_writer, kSdpMasRecordPropSupportedFeatures,
                 record.supported_features);
  WriteDictEntry(&array_writer, kSdpMasRecordPropSupportedMessageTypes,
                 record.supported_message_types);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpMnsRecord::hdr)},
       {kSdpMnsRecordPropSupportedFeatures,
        CreateFieldReader(&BtSdpMnsRecord::supported_features)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpMnsRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);
  WriteDictEntry(&array_writer, kSdpMnsRecordPropSupportedFeatures,
                 record.supported_features);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpPseRecord::hdr)},
       {kSdpPseRecordPropSupportedFeatures,
        CreateFieldReader(&BtSdpPseRecord::supported_features)},
       {kSdpPseRecordPropSupportedRepositories,
        CreateFieldReader(&BtSdpPseRecord::supported_repositories)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpPseRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);
  WriteDictEntry(&array_writer, kSdpPseRecordPropSupportedFeatures,
                 record.supported_features);
  WriteDictEntry(&array_writer, kSdpPseRecordPropSupportedRepositories,
                 record.supported_repositories);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpPceRecord::hdr)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpPceRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpOpsRecord::hdr)},
       {kSdpOpsRecordPropSupportedFormatsListLen,
        CreateFieldReader(&BtSdpOpsRecord::supported_formats_list_len)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpOpsRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);
  WriteDictEntry(&array_writer, kSdpOpsRecordPropSupportedFormatsListLen,
                 record.supported_formats_list_len);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpSapRecord::hdr)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpSapRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);

  writer->CloseContainer(&array_writer);
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
      {{kSdpRecordPropHdr, CreateFieldReader(&BtSdpDipRecord::hdr)},
       {kSdpDipRecordPropSpecId, CreateFieldReader(&BtSdpDipRecord::spec_id)},
       {kSdpDipRecordPropVendor, CreateFieldReader(&BtSdpDipRecord::vendor)},
       {kSdpDipRecordPropVendorIdSource,
        CreateFieldReader(&BtSdpDipRecord::vendor_id_source)},
       {kSdpDipRecordPropProduct, CreateFieldReader(&BtSdpDipRecord::product)},
       {kSdpDipRecordPropVersion, CreateFieldReader(&BtSdpDipRecord::version)},
       {kSdpDipRecordPropPrimaryRecord,
        CreateFieldReader(&BtSdpDipRecord::primary_record)}});
  return struct_reader.ReadDBusParam(reader, record);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpDipRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, kSdpRecordPropHdr, record.hdr);
  WriteDictEntry(&array_writer, kSdpDipRecordPropSpecId, record.spec_id);
  WriteDictEntry(&array_writer, kSdpDipRecordPropVendor, record.vendor);
  WriteDictEntry(&array_writer, kSdpDipRecordPropVendorIdSource,
                 record.vendor_id_source);
  WriteDictEntry(&array_writer, kSdpDipRecordPropProduct, record.product);
  WriteDictEntry(&array_writer, kSdpDipRecordPropVersion, record.version);
  WriteDictEntry(&array_writer, kSdpDipRecordPropPrimaryRecord,
                 record.primary_record);

  writer->CloseContainer(&array_writer);
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

  if (!base::Contains(unparsed_args, kTypeKey)) {
    LOG(ERROR) << "BtSdpRecord did not contain type identifier";
    return false;
  }

  if (!base::Contains(unparsed_args, kVariantValueKey)) {
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

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BtSdpRecord& record) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  if (absl::holds_alternative<BtSdpHeaderOverlay>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kRaw));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpHeaderOverlay>(record));
  } else if (absl::holds_alternative<BtSdpMasRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kMapMas));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpMasRecord>(record));
  } else if (absl::holds_alternative<BtSdpMnsRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kMapMns));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpMnsRecord>(record));
  } else if (absl::holds_alternative<BtSdpPseRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kPbapPse));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpPseRecord>(record));
  } else if (absl::holds_alternative<BtSdpPceRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kPbapPce));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpPceRecord>(record));
  } else if (absl::holds_alternative<BtSdpOpsRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kOppServer));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpOpsRecord>(record));
  } else if (absl::holds_alternative<BtSdpSapRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kSapServer));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpSapRecord>(record));
  } else if (absl::holds_alternative<BtSdpDipRecord>(record)) {
    WriteDictEntry(&array_writer, kTypeKey,
                   static_cast<uint32_t>(BtSdpType::kDip));
    WriteDictEntry(&array_writer, kVariantValueKey,
                   absl::get<BtSdpDipRecord>(record));
  }

  writer->CloseContainer(&array_writer);
}

}  // namespace floss
