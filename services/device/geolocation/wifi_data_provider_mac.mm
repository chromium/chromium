// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_mac.h"

#import <CoreWLAN/CoreWLAN.h>
#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "services/device/geolocation/wifi_data_provider_common.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"

namespace device {

namespace {

class CoreWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  CoreWlanApi() {
    wifi_client_.reset([CWWiFiClient sharedWiFiClient],
                       base::scoped_policy::RETAIN);
  }

  CoreWlanApi(const CoreWlanApi&) = delete;
  CoreWlanApi& operator=(const CoreWlanApi&) = delete;

  // WlanApiInterface:
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  base::scoped_nsobject<CWWiFiClient> wifi_client_;
};

bool CoreWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  @autoreleasepool {
    NSArray<CWInterface*>* interfaces = [wifi_client_ interfaces];
    NSUInteger interface_error_count = 0;
    for (CWInterface* interface in interfaces) {
      NSError* err = nil;
      NSSet<CWNetwork*>* scan = [interface scanForNetworksWithName:nil
                                                             error:&err];
      const int error_code = [err code];
      const int count = [scan count];
      // We could get an error code but count != 0 if the scan was interrupted,
      // for example. For our purposes this is not fatal, so process as normal.
      if (error_code && count == 0) {
        DLOG(WARNING) << interface.interfaceName
                      << ": CoreWLAN scan failed with error " << error_code;
        ++interface_error_count;
        continue;
      }

      DVLOG(1) << interface.interfaceName << ": found " << count << " wifi APs";

      for (CWNetwork* network in scan) {
        DCHECK(network);
        AccessPointData access_point_data;
        // -[CWNetwork bssid] uses colons to separate the components of the MAC
        // address, but AccessPointData requires they be separated with a dash.
        access_point_data.mac_address = base::SysNSStringToUTF16(
            [[network bssid] stringByReplacingOccurrencesOfString:@":"
                                                       withString:@"-"]);
        access_point_data.radio_signal_strength = [network rssiValue];
        access_point_data.channel = [[network wlanChannel] channelNumber];
        access_point_data.signal_to_noise =
            access_point_data.radio_signal_strength -
            [network noiseMeasurement];
        access_point_data.ssid = base::SysNSStringToUTF16([network ssid]);
        data->insert(access_point_data);
      }
    }

    // Return true even if some interfaces failed to scan, so long as at least
    // one interface did not fail.
    return interface_error_count == 0 ||
           [interfaces count] > interface_error_count;
  }
}

// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 120000;                // 2 mins
const int kNoChangePollingInterval = 300000;               // 5 mins
const int kTwoNoChangePollingInterval = 600000;            // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000;  // 20s

}  // namespace

// static
WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderMac();
}

WifiDataProviderMac::WifiDataProviderMac() {}

WifiDataProviderMac::~WifiDataProviderMac() {}

std::unique_ptr<WifiDataProviderMac::WlanApiInterface>
WifiDataProviderMac::CreateWlanApi() {
  return std::make_unique<CoreWlanApi>();
}

std::unique_ptr<WifiPollingPolicy> WifiDataProviderMac::CreatePollingPolicy() {
  return std::make_unique<GenericWifiPollingPolicy<
      kDefaultPollingInterval, kNoChangePollingInterval,
      kTwoNoChangePollingInterval, kNoWifiPollingIntervalMilliseconds>>();
}

}  // namespace device
