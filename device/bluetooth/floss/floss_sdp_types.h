// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_SDP_TYPES_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_SDP_TYPES_H_

#include <string>

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

enum class BtSdpType : uint32_t {
  kRaw = 0,
  kMapMas = 1,
  kMapMns = 2,
  kPbapPse = 3,
  kPbapPce = 4,
  kOppServer = 5,
  kSapServer = 6,
  kDip = 7
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpHeaderOverlay {
  BtSdpHeaderOverlay();
  BtSdpHeaderOverlay(const BtSdpHeaderOverlay& other);
  ~BtSdpHeaderOverlay();

  BtSdpType sdp_type;
  device::BluetoothUUID uuid;
  uint32_t service_name_length;
  std::string service_name;
  int32_t rfcomm_channel_number;
  int32_t l2cap_psm;
  int32_t profile_version;

  int32_t user1_len;
  std::vector<uint8_t> user1_data;
  int32_t user2_len;
  std::vector<uint8_t> user2_data;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpMasRecord {
  BtSdpHeaderOverlay hdr;
  uint32_t mas_instance_id;
  uint32_t supported_features;
  uint32_t supported_message_types;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpMnsRecord {
  BtSdpHeaderOverlay hdr;
  uint32_t supported_features;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpPseRecord {
  BtSdpHeaderOverlay hdr;
  uint32_t supported_features;
  uint32_t supported_repositories;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpPceRecord {
  BtSdpHeaderOverlay hdr;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpOpsRecord {
  BtSdpHeaderOverlay hdr;
  int32_t supported_formats_list_len;
  // TODO: supported_formats_list
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpSapRecord {
  BtSdpHeaderOverlay hdr;
};

struct DEVICE_BLUETOOTH_EXPORT BtSdpDipRecord {
  BtSdpHeaderOverlay hdr;
  uint16_t spec_id;
  uint16_t vendor;
  uint16_t vendor_id_source;
  uint16_t product;
  uint16_t version;
  bool primary_record;
};

using BtSdpRecord = absl::variant<BtSdpHeaderOverlay,
                                  BtSdpMasRecord,
                                  BtSdpMnsRecord,
                                  BtSdpPseRecord,
                                  BtSdpPceRecord,
                                  BtSdpOpsRecord,
                                  BtSdpSapRecord,
                                  BtSdpDipRecord>;
std::optional<floss::BtSdpHeaderOverlay> DEVICE_BLUETOOTH_EXPORT
GetHeaderOverlayFromSdpRecord(const floss::BtSdpRecord& record);

std::optional<device::BluetoothUUID> DEVICE_BLUETOOTH_EXPORT
GetUUIDFromSdpRecord(const floss::BtSdpRecord& record);

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_SDP_TYPES_H_
