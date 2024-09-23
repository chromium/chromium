// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/geolocation/wifi_data_provider_common_win.h"

#include <assert.h>
#include <stdint.h>

#include "services/device/geolocation/wifi_data_provider_common.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {

bool ConvertToAccessPointData(const NDIS_WLAN_BSSID& data,
                              mojom::AccessPointData* access_point_data) {
  // Currently we get only MAC address and signal strength.
  // TODO(steveblock): Work out how to get age, channel and signal-to-noise.
  DCHECK(access_point_data);
  access_point_data->mac_address = MacAddressAsString(data.MacAddress);
  access_point_data->radio_signal_strength = data.Rssi;
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
    mojom::AccessPointData access_point_data;
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
