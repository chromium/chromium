// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/power_monitor_test_impl.h"

#include <memory>
#include <utility>

#include "content/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void PowerMonitorTestImpl::MakeSelfOwnedReceiver(
    mojo::PendingReceiver<mojom::PowerMonitorTest> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<PowerMonitorTestImpl>(),
                              std::move(receiver));
}

PowerMonitorTestImpl::PowerMonitorTestImpl() {
  base::PowerMonitor::AddObserver(this);
}

PowerMonitorTestImpl::~PowerMonitorTestImpl() {
  base::PowerMonitor::RemoveObserver(this);
}

void PowerMonitorTestImpl::QueryNextState(QueryNextStateCallback callback) {
  // Do not allow overlapping call.
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  if (need_to_report_)
    ReportState();
}

void PowerMonitorTestImpl::OnPowerStateChange(bool on_battery_power) {
  on_battery_power_ = on_battery_power;
  need_to_report_ = true;

  if (!callback_.is_null())
    ReportState();
}

void PowerMonitorTestImpl::ReportState() {
  std::move(callback_).Run(on_battery_power_);
  need_to_report_ = false;
}

}  // namespace content
