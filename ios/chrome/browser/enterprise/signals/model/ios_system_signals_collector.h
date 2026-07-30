// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_SYSTEM_SIGNALS_COLLECTOR_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_SYSTEM_SIGNALS_COLLECTOR_H_

#import "base/memory/weak_ptr.h"
#import "base/system/sys_info.h"
#import "components/device_signals/core/browser/base_signals_collector.h"
#import "components/device_signals/core/browser/signals_types.h"

class IOSSystemSignalsCollector : public device_signals::BaseSignalsCollector {
 public:
  IOSSystemSignalsCollector();
  ~IOSSystemSignalsCollector() override;

  IOSSystemSignalsCollector(const IOSSystemSignalsCollector&) = delete;
  IOSSystemSignalsCollector& operator=(const IOSSystemSignalsCollector&) =
      delete;

 private:
  void GetOsSignals(device_signals::UserPermission permission,
                    const device_signals::SignalsAggregationRequest& request,
                    device_signals::SignalsAggregationResponse& response,
                    base::OnceClosure done_closure);

  void OnHardwareInfoReceived(
      device_signals::SignalsAggregationResponse& response,
      std::unique_ptr<device_signals::OsSignalsResponse> signal_response,
      base::OnceClosure done_closure,
      base::SysInfo::HardwareInfo hardware_info);

  base::WeakPtrFactory<IOSSystemSignalsCollector> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_SYSTEM_SIGNALS_COLLECTOR_H_
