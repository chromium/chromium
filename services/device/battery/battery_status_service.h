// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_SERVICE_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/macros.h"
#include "services/device/public/mojom/battery_status.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {
class BatteryStatusManager;

class BatteryStatusService {
 public:
  typedef base::Callback<void(const mojom::BatteryStatus&)>
      BatteryUpdateCallback;
  typedef base::CallbackList<void(const mojom::BatteryStatus&)>
      BatteryUpdateCallbackList;
  typedef BatteryUpdateCallbackList::Subscription BatteryUpdateSubscription;

  // Returns the BatteryStatusService singleton.
  static BatteryStatusService* GetInstance();

  // NOTE: These must be public due to internal stashing of the global
  // BatteryStatusService object in an std::unique_ptr. Clients should use only
  // the static GetInstance() method above.
  BatteryStatusService();
  virtual ~BatteryStatusService();

  // Adds a callback to receive battery status updates.  Must be called on the
  // main thread. The callback itself will be called on the main thread as well.
  // NOTE: The callback may be run before AddCallback returns!
  std::unique_ptr<BatteryUpdateSubscription> AddCallback(
      const BatteryUpdateCallback& callback);

  // Gracefully clean-up.
  void Shutdown();

  // Injects a custom battery status manager for testing purposes.
  void SetBatteryManagerForTesting(
      std::unique_ptr<BatteryStatusManager> test_battery_manager);

  // Returns callback to invoke when battery is changed. Used for testing.
  const BatteryUpdateCallback& GetUpdateCallbackForTesting() const;

 private:
  friend class BatteryStatusServiceTest;

  // Updates current battery status and sends new status to interested
  // render processes. Can be called on any thread via a callback.
  void NotifyConsumers(const mojom::BatteryStatus& status);
  void NotifyConsumersOnMainThread(const mojom::BatteryStatus& status);
  void ConsumersChanged();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<BatteryStatusManager> battery_fetcher_;
  BatteryUpdateCallbackList callback_list_;
  BatteryUpdateCallback update_callback_;
  mojom::BatteryStatus status_;
  bool status_updated_;
  bool is_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_SERVICE_H_
