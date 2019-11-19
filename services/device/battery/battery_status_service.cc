// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_manager.h"

namespace device {

BatteryStatusService::BatteryStatusService()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      update_callback_(base::Bind(&BatteryStatusService::NotifyConsumers,
                                  base::Unretained(this))),
      status_updated_(false),
      is_shutdown_(false) {
  callback_list_.set_removal_callback(base::Bind(
      &BatteryStatusService::ConsumersChanged, base::Unretained(this)));
}

BatteryStatusService::~BatteryStatusService() {
  Shutdown();
}

BatteryStatusService* BatteryStatusService::GetInstance() {
  // On embedder teardown, the BatteryStatusService object needs to shut down
  // certain parts of its state to avoid DCHECKs going off on Windows (see.
  // https://crbug.com/794105 for details). However, when used in the context of
  // the browser the Device Service is not guaranteed to have its destructor
  // run, as the sequence on which it runs is stopped before the task posted to
  // run the Device Service destructor is run. Hence, stash the
  // BatteryStatusService singleton in a SequenceLocalStorageSlot to ensure that
  // its destructor (and consequently Shutdown() method) are run on embedder
  // teardown. Unfortunately, this shutdown is currently not possible on
  // ChromeOS: crbug.com/856771 presents a crash that performing this shutdown
  // introduces on that platform, as it causes the BatteryStatusService instance
  // to be shut down after the DBusThreadManager global instance, on which it
  // implicitly depends.
#if defined(OS_CHROMEOS)
  static base::NoDestructor<BatteryStatusService> service_wrapper;
  return service_wrapper.get();
#else
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<BatteryStatusService>>>
      service_local_storage_slot_wrapper;

  auto& service_local_storage_slot = *service_local_storage_slot_wrapper;

  if (!service_local_storage_slot)
    service_local_storage_slot.emplace(new BatteryStatusService());

  return service_local_storage_slot->get();
#endif
}

std::unique_ptr<BatteryStatusService::BatteryUpdateSubscription>
BatteryStatusService::AddCallback(const BatteryUpdateCallback& callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_shutdown_);

  if (!battery_fetcher_)
    battery_fetcher_ = BatteryStatusManager::Create(update_callback_);

  if (callback_list_.empty()) {
    bool success = battery_fetcher_->StartListeningBatteryChange();
    // On failure pass the default values back.
    if (!success)
      callback.Run(mojom::BatteryStatus());
  }

  if (status_updated_) {
    // Send recent status to the new callback if already available.
    callback.Run(status_);
  }

  return callback_list_.Add(callback);
}

void BatteryStatusService::ConsumersChanged() {
  if (is_shutdown_)
    return;

  if (callback_list_.empty()) {
    battery_fetcher_->StopListeningBatteryChange();
    status_updated_ = false;
  }
}

void BatteryStatusService::NotifyConsumers(const mojom::BatteryStatus& status) {
  DCHECK(!is_shutdown_);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BatteryStatusService::NotifyConsumersOnMainThread,
                     base::Unretained(this), status));
}

void BatteryStatusService::NotifyConsumersOnMainThread(
    const mojom::BatteryStatus& status) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (callback_list_.empty())
    return;

  status_ = status;
  status_updated_ = true;
  callback_list_.Notify(status_);
}

void BatteryStatusService::Shutdown() {
  if (!callback_list_.empty())
    battery_fetcher_->StopListeningBatteryChange();
  battery_fetcher_.reset();
  is_shutdown_ = true;
}

const BatteryStatusService::BatteryUpdateCallback&
BatteryStatusService::GetUpdateCallbackForTesting() const {
  return update_callback_;
}

void BatteryStatusService::SetBatteryManagerForTesting(
    std::unique_ptr<BatteryStatusManager> test_battery_manager) {
  battery_fetcher_ = std::move(test_battery_manager);
  status_ = mojom::BatteryStatus();
  status_updated_ = false;
  is_shutdown_ = false;
  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

}  // namespace device
