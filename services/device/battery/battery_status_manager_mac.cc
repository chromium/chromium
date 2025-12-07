// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace device {

namespace {

typedef BatteryStatusService::BatteryUpdateCallback BatteryCallback;

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is NULL or it could not be converted to SInt64.
SInt64 GetValueAsSInt64(CFDictionaryRef description,
                        CFStringRef key,
                        SInt64 default_value) {
  CFNumberRef number =
      base::apple::GetValueFromDictionary<CFNumberRef>(description, key);
  SInt64 value;

  if (number && CFNumberGetValue(number, kCFNumberSInt64Type, &value))
    return value;

  return default_value;
}

bool GetValueAsBoolean(CFDictionaryRef description,
                       CFStringRef key,
                       bool default_value) {
  CFBooleanRef boolean =
      base::apple::GetValueFromDictionary<CFBooleanRef>(description, key);

  return boolean ? CFBooleanGetValue(boolean) : default_value;
}

bool CFStringsAreEqual(CFStringRef string1, CFStringRef string2) {
  if (!string1 || !string2)
    return false;
  return CFStringCompare(string1, string2, 0) == kCFCompareEqualTo;
}

void FetchBatteryStatus(CFDictionaryRef description,
                        mojom::BatteryStatus* status) {
  CFStringRef current_state = base::apple::GetValueFromDictionary<CFStringRef>(
      description, CFSTR(kIOPSPowerSourceStateKey));

  bool on_battery_power =
      CFStringsAreEqual(current_state, CFSTR(kIOPSBatteryPowerValue));
  bool is_charging =
      GetValueAsBoolean(description, CFSTR(kIOPSIsChargingKey), true);
  bool is_charged =
      GetValueAsBoolean(description, CFSTR(kIOPSIsChargedKey), false);

  status->charging = !on_battery_power || is_charging;

  SInt64 current_capacity =
      GetValueAsSInt64(description, CFSTR(kIOPSCurrentCapacityKey), -1);
  SInt64 max_capacity =
      GetValueAsSInt64(description, CFSTR(kIOPSMaxCapacityKey), -1);

  // Set level if it is available and valid. Otherwise leave the default value,
  // which is 1.
  if (current_capacity != -1 && max_capacity != -1 &&
      current_capacity <= max_capacity && max_capacity != 0) {
    status->level = current_capacity / static_cast<double>(max_capacity);
  }

  if (is_charging) {
    SInt64 charging_time =
        GetValueAsSInt64(description, CFSTR(kIOPSTimeToFullChargeKey), -1);

    // Battery is charging: set the charging time if it's available, otherwise
    // set to +infinity.
    status->charging_time = charging_time != -1
                                ? base::Minutes(charging_time).InSeconds()
                                : std::numeric_limits<double>::infinity();
  } else {
    // Battery is not charging.
    // Set chargingTime to +infinity if the battery is not charged. Otherwise
    // leave the default value, which is 0.
    if (!is_charged)
      status->charging_time = std::numeric_limits<double>::infinity();

    // Set dischargingTime if it's available and valid, i.e. when on battery
    // power. Otherwise leave the default value, which is +infinity.
    if (on_battery_power) {
      SInt64 discharging_time =
          GetValueAsSInt64(description, CFSTR(kIOPSTimeToEmptyKey), -1);
      if (discharging_time != -1) {
        status->discharging_time = base::Minutes(discharging_time).InSeconds();
      }
    }
  }
}

std::vector<mojom::BatteryStatus> GetInternalBatteriesStates(bool may_block) {
  std::vector<mojom::BatteryStatus> internal_sources;

  // This function is known to block but cannot always be tagged as such right
  // now because it might run on the UI thread. When running on the ThreadPool
  // though it should be appropriately tagged.
  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  if (may_block) {
    scoped_blocking_call.emplace(FROM_HERE, base::BlockingType::MAY_BLOCK);
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> info(IOPSCopyPowerSourcesInfo());
  base::apple::ScopedCFTypeRef<CFArrayRef> power_sources_list(
      IOPSCopyPowerSourcesList(info.get()));
  CFIndex count = CFArrayGetCount(power_sources_list.get());

  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef description = IOPSGetPowerSourceDescription(
        info.get(), CFArrayGetValueAtIndex(power_sources_list.get(), i));

    if (!description)
      continue;

    CFStringRef transport_type =
        base::apple::GetValueFromDictionary<CFStringRef>(
            description, CFSTR(kIOPSTransportTypeKey));

    bool internal_source =
        CFStringsAreEqual(transport_type, CFSTR(kIOPSInternalType));
    bool source_present =
        GetValueAsBoolean(description, CFSTR(kIOPSIsPresentKey), false);

    if (internal_source && source_present) {
      mojom::BatteryStatus status;
      FetchBatteryStatus(description, &status);
      internal_sources.push_back(status);
    }
  }

  return internal_sources;
}

void HandleNewBatteryStatus(const BatteryCallback& callback,
                            std::vector<mojom::BatteryStatus> batteries) {
  if (batteries.empty()) {
    callback.Run(mojom::BatteryStatus());
    return;
  }

  // TODO(timvolodine): implement the case when there are multiple internal
  // sources, e.g. when multiple batteries are present. Currently this will
  // fail a DCHECK.
  DCHECK_EQ(1U, batteries.size());
  callback.Run(batteries.front());
}

void OnBatteryStatusChangedAsync(const BatteryCallback& callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetInternalBatteriesStates, true),
      base::BindOnce(&HandleNewBatteryStatus, callback));
}

void OnBatteryStatusChanged(const BatteryCallback& callback) {
  HandleNewBatteryStatus(callback, GetInternalBatteriesStates(false));
}

class BatteryStatusObserver {
 public:
  explicit BatteryStatusObserver(const BatteryCallback& callback)
      : callback_(callback) {}

  BatteryStatusObserver(const BatteryStatusObserver&) = delete;
  BatteryStatusObserver& operator=(const BatteryStatusObserver&) = delete;

  ~BatteryStatusObserver() { DCHECK(!notifier_run_loop_source_); }

  void Start() {
    if (notifier_run_loop_source_)
      return;

    notifier_run_loop_source_.reset(IOPSNotificationCreateRunLoopSource(
        CallOnBatteryStatusChanged, static_cast<void*>(&callback_)));
    if (!notifier_run_loop_source_) {
      LOG(ERROR) << "Failed to create battery status notification run loop";
      // Make sure to execute to callback with the default values.
      callback_.Run(mojom::BatteryStatus());
      return;
    }

    CallOnBatteryStatusChanged(static_cast<void*>(&callback_));
    CFRunLoopAddSource(CFRunLoopGetCurrent(), notifier_run_loop_source_.get(),
                       kCFRunLoopDefaultMode);
  }

  void Stop() {
    if (!notifier_run_loop_source_)
      return;

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                          notifier_run_loop_source_.get(),
                          kCFRunLoopDefaultMode);
    notifier_run_loop_source_.reset();
  }

 private:
  static void CallOnBatteryStatusChanged(void* callback) {
    if (base::FeatureList::IsEnabled(base::features::kReducePPMs)) {
      OnBatteryStatusChangedAsync(*static_cast<BatteryCallback*>(callback));
    } else {
      OnBatteryStatusChanged(*static_cast<BatteryCallback*>(callback));
    }
  }

  BatteryCallback callback_;
  base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> notifier_run_loop_source_;
};

class BatteryStatusManagerMac : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerMac(const BatteryCallback& callback)
      : notifier_(std::make_unique<BatteryStatusObserver>(callback)) {}

  BatteryStatusManagerMac(const BatteryStatusManagerMac&) = delete;
  BatteryStatusManagerMac& operator=(const BatteryStatusManagerMac&) = delete;

  ~BatteryStatusManagerMac() override { notifier_->Stop(); }

  // BatteryStatusManager:
  bool StartListeningBatteryChange() override {
    notifier_->Start();
    return true;
  }

  void StopListeningBatteryChange() override { notifier_->Stop(); }

 private:
  std::unique_ptr<BatteryStatusObserver> notifier_;
};

}  // namespace

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::make_unique<BatteryStatusManagerMac>(callback);
}

}  // namespace device
