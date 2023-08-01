// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <memory>

#include "base/functional/callback.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace device {
namespace {
typedef BatteryStatusService::BatteryUpdateCallback BatteryCallback;

mojom::BatteryStatus GetInternalBatteriesStates() {
  mojom::BatteryStatus status;
  switch ([UIDevice currentDevice].batteryState) {
    case UIDeviceBatteryStateFull:
      status.charging = true;
      break;
    case UIDeviceBatteryStateUnplugged:
      status.charging = false;
      break;
    case UIDeviceBatteryStateCharging:
      status.charging = true;
      break;
    case UIDeviceBatteryStateUnknown:
      status.charging = false;
      break;
  }
  if ([UIDevice currentDevice].batteryState != UIDeviceBatteryStateUnknown) {
    status.level = [UIDevice currentDevice].batteryLevel;
  }
  // It's not recommended to calculate time from
  // https://developer.apple.com/documentation/uikit/uidevicebatteryleveldidchangenotification
  // Set `charging_time` to +Infinity if not fully charged, otherwise leave the
  // default value, which is 0.
  if (status.charging && status.level < 1) {
    status.charging_time = std::numeric_limits<double>::infinity();
  }

  return status;
}

}  // namespace
}  // namespace device

// Starts and stops monitoring battery status.
@interface BatteryNotification : NSObject {
 @private
  device::BatteryCallback _callback;
}
- (instancetype)initWithCallback:(const device::BatteryCallback&)callback;
- (void)startNotification;
- (void)stopNotification;
@end

@implementation BatteryNotification
- (instancetype)initWithCallback:(const device::BatteryCallback&)callback {
  if (!(self = [super init])) {
    return nil;
  }
  _callback = callback;
  return self;
}

- (void)startNotification {
  [[UIDevice currentDevice] setBatteryMonitoringEnabled:YES];

  // Update the current status.
  self->_callback.Run(device::GetInternalBatteriesStates());

  // Add observers for battery state and level changes.
  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIDeviceBatteryStateDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* notification) {
                // BatteryState has changed.
                self->_callback.Run(device::GetInternalBatteriesStates());
              }];

  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIDeviceBatteryLevelDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* notification) {
                // BatteryLevel has changed.
                self->_callback.Run(device::GetInternalBatteriesStates());
              }];
}

- (void)stopNotification {
  [[UIDevice currentDevice] setBatteryMonitoringEnabled:NO];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIDeviceBatteryStateDidChangeNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIDeviceBatteryLevelDidChangeNotification
              object:nil];
}
@end

namespace device {

namespace {

class BatteryStatusManagerIOS : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerIOS(const BatteryCallback& callback) {
    notification_ = [[BatteryNotification alloc] initWithCallback:callback];
  }

  BatteryStatusManagerIOS(const BatteryStatusManagerIOS&) = delete;
  BatteryStatusManagerIOS& operator=(const BatteryStatusManagerIOS&) = delete;

  ~BatteryStatusManagerIOS() override { StopListeningBatteryChange(); }

  // BatteryStatusManager:
  bool StartListeningBatteryChange() override {
    [notification_ startNotification];
    return true;
  }

  void StopListeningBatteryChange() override {
    [notification_ stopNotification];
  }

 private:
  __strong BatteryNotification* notification_;
};

}  // namespace

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::make_unique<BatteryStatusManagerIOS>(callback);
}

}  // namespace device
