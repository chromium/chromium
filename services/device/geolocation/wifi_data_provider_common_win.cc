// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_common_win.h"

#include <assert.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "services/device/geolocation/wifi_data_provider_common.h"

namespace device {

bool ConvertToAccessPointData(const NDIS_WLAN_BSSID& data,
                              AccessPointData* access_point_data) {
  // Currently we get only MAC address, signal strength and SSID.
  // TODO(steveblock): Work out how to get age, channel and signal-to-noise.
  DCHECK(access_point_data);
  access_point_data->mac_address = MacAddressAsString16(data.MacAddress);
  access_point_data->radio_signal_strength = data.Rssi;
  // Note that _NDIS_802_11_SSID::Ssid::Ssid is not null-terminated.
  base::UTF8ToUTF16(reinterpret_cast<const char*>(data.Ssid.Ssid),
                    data.Ssid.SsidLength, &access_point_data->ssid);
  return true;
}

int GetDataFromBssIdList(const NDIS_802_11_BSSID_LIST& bss_id_list,
                         int list_size,
                         WifiData::AccessPointDataSet* data) {
  // Walk through the BSS IDs.
  int found = 0;
  const uint8_t* iterator =
      reinterpret_cast<const uint8_t*>(&bss_id_list.Bssid[0]);
  const uint8_t* end_of_buffer =
      reinterpret_cast<const uint8_t*>(&bss_id_list) + list_size;
  for (int i = 0; i < static_cast<int>(bss_id_list.NumberOfItems); ++i) {
    const NDIS_WLAN_BSSID* bss_id =
        reinterpret_cast<const NDIS_WLAN_BSSID*>(iterator);
    // Check that the length of this BSS ID is reasonable.
    if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) ||
        iterator + bss_id->Length > end_of_buffer) {
      break;
    }
    AccessPointData access_point_data;
    if (ConvertToAccessPointData(*bss_id, &access_point_data)) {
      data->insert(access_point_data);
      ++found;
    }
    // Move to the next BSS ID.
    iterator += bss_id->Length;
  }
  return found;
}

}  // namespace device
