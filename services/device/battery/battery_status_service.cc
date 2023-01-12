// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_manager.h"

namespace device {

BatteryStatusService::BatteryStatusService()
    : main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      update_callback_(
          base::BindRepeating(&BatteryStatusService::NotifyConsumers,
                              base::Unretained(this))),
      status_updated_(false),
      is_shutdown_(false) {
  callback_list_.set_removal_callback(base::BindRepeating(
      &BatteryStatusService::ConsumersChanged, base::Unretained(this)));
}

BatteryStatusService::~BatteryStatusService() = default;

BatteryStatusService* BatteryStatusService::GetInstance() {
  static base::NoDestructor<BatteryStatusService> service_wrapper;
  return service_wrapper.get();
}

base::CallbackListSubscription BatteryStatusService::AddCallback(
    const BatteryUpdateCallback& callback) {
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
  main_thread_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace device
