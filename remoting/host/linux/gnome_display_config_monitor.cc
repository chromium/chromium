// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config_monitor.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/gnome_display_config.h"

namespace remoting {

GnomeDisplayConfigMonitor::Subscription::Subscription() = default;
GnomeDisplayConfigMonitor::Subscription::~Subscription() = default;

GnomeDisplayConfigMonitor::GnomeDisplayConfigMonitor(
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client)
    : display_config_client_(display_config_client) {
  Start();
}

GnomeDisplayConfigMonitor::~GnomeDisplayConfigMonitor() = default;

void GnomeDisplayConfigMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    monitors_changed_subscription_ =
        display_config_client_->SubscribeMonitorsChanged(
            base::BindRepeating(&GnomeDisplayConfigMonitor::QueryDisplayConfig,
                                base::Unretained(this)));
  }
  QueryDisplayConfig();
}

void GnomeDisplayConfigMonitor::QueryDisplayConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    display_config_client_->GetMonitorsConfig(
        base::BindOnce(&GnomeDisplayConfigMonitor::OnGnomeDisplayConfigReceived,
                       base::Unretained(this)));
  }
}

std::unique_ptr<GnomeDisplayConfigMonitor::Subscription>
GnomeDisplayConfigMonitor::AddCallback(Callback callback,
                                       bool call_with_current_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto subscription = base::WrapUnique(new Subscription());
  subscription->subscription_ = callbacks_.Add(callback);
  if (call_with_current_config && current_config_.has_value()) {
    bool schedule_call = callbacks_pending_current_config_.empty();
    // We can't just bind `current_config_` to the callback, since a new config
    // could be received before the posted task is executed.
    callbacks_pending_current_config_.AddUnsafe(base::BindOnce(
        [](base::WeakPtr<Subscription> subscription, Callback callback,
           const GnomeDisplayConfig& config) {
          // Don't run the callback if the subscription object has already been
          // discarded.
          if (subscription) {
            callback.Run(config);
          }
        },
        subscription->weak_factory_.GetWeakPtr(), callback));
    if (schedule_call) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&GnomeDisplayConfigMonitor::CallWithCurrentConfig,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  return subscription;
}

base::WeakPtr<GnomeDisplayConfigMonitor>
GnomeDisplayConfigMonitor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GnomeDisplayConfigMonitor::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_config_ = std::move(config);
  callbacks_pending_current_config_.Clear();
  callbacks_.Notify(*current_config_);
}

void GnomeDisplayConfigMonitor::CallWithCurrentConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_config_.has_value());

  // Note about reentrancy: registered callbacks may potentially add more
  // callbacks to the callback list, which is fine. Per documentation of
  // CallbackListBase::Notify, callbacks are not pruned until the outermost
  // iteration completes, so `schedule_call` in
  // GnomeDisplayConfigMonitor::AddCallback will be false, and the Notify call
  // will run all callbacks added during the iteration.
  callbacks_pending_current_config_.Notify(*current_config_);
}

}  // namespace remoting
