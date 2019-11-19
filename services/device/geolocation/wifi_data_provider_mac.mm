// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_mac.h"

#import <CoreWLAN/CoreWLAN.h>
#import <Foundation/Foundation.h>

// This file uses the deprecated CWInterface API, but CWWiFiClient appears to be
// different in ways that are relevant to this code, so for now ignore the
// deprecation. See <https://crbug.com/841631>.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "services/device/geolocation/wifi_data_provider_common.h"
#include "services/device/geolocation/wifi_data_provider_manager.h"

#if !defined(MAC_OS_X_VERSION_10_15)
// This API is so deprecated that this symbol is no longer present at all in the
// 10.15 SDK. For the moment, hack this functionality out entirely when building
// with the 10.15 SDK.
// https://crbug.com/1022821
extern "C" NSString* const kCWScanKeyMerge;
#endif

@interface CWInterface (Private)
- (NSArray*)scanForNetworksWithParameters:(NSDictionary*)params
                                    error:(NSError**)error;
@end

namespace device {

namespace {

class CoreWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  CoreWlanApi() {}

  // WlanApiInterface:
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CoreWlanApi);
};

bool CoreWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  @autoreleasepool {  // Initialize the scan parameters with scan key merging
                      // disabled, so we get
    // every AP listed in the scan without any SSID de-duping logic.
#if defined(MAC_OS_X_VERSION_10_15)
    NSDictionary* params = @{};
#else
    NSDictionary* params = @{kCWScanKeyMerge : @NO};
#endif

    NSSet* supported_interfaces = [CWInterface interfaceNames];
    NSUInteger interface_error_count = 0;
    for (NSString* interface_name in supported_interfaces) {
      CWInterface* corewlan_interface =
          [CWInterface interfaceWithName:interface_name];
      if (!corewlan_interface) {
        DLOG(WARNING) << interface_name << ": initWithName failed";
        ++interface_error_count;
        continue;
      }

      const base::TimeTicks start_time = base::TimeTicks::Now();

      NSError* err = nil;
      NSArray* scan = [corewlan_interface scanForNetworksWithParameters:params
                                                                  error:&err];
      const int error_code = [err code];
      const int count = [scan count];
      // We could get an error code but count != 0 if the scan was interrupted,
      // for example. For our purposes this is not fatal, so process as normal.
      if (error_code && count == 0) {
        DLOG(WARNING) << interface_name << ": CoreWLAN scan failed with error "
                      << error_code;
        ++interface_error_count;
        continue;
      }

      const base::TimeDelta duration = base::TimeTicks::Now() - start_time;

      UMA_HISTOGRAM_CUSTOM_TIMES("Net.Wifi.ScanLatency", duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1), 100);

      DVLOG(1) << interface_name << ": found " << count << " wifi APs";

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

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.Wifi.InterfaceCount",
        [supported_interfaces count] - interface_error_count, 1, 5, 6);

    // Return true even if some interfaces failed to scan, so long as at least
    // one interface did not fail.
    return interface_error_count == 0 ||
           [supported_interfaces count] > interface_error_count;
  }
}

// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 120000;                // 2 mins
const int kNoChangePollingInterval = 300000;               // 5 mins
const int kTwoNoChangePollingInterval = 600000;            // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000;  // 20s

}  // namespace

// static
WifiDataProvider* WifiDataProviderManager::DefaultFactoryFunction() {
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

#pragma clang diagnostic pop
