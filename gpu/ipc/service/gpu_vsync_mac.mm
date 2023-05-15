// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_vsync_mac.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"

namespace gpu {
namespace {
BASE_FEATURE(kForceGpuVSyncTimerForDebugging,
             "ForceGpuVSyncTimerForDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

GpuVSyncMac::GpuVSyncMac(viz::GpuVSyncCallback vsync_callback)
    : vsync_callback_(vsync_callback) {}

GpuVSyncMac::~GpuVSyncMac() {
  if (display_link_mac_) {
    vsync_callback_mac_ = nullptr;
    display_link_mac_ = nullptr;
  } else {
    timer_based_vsync_mac_->RemoveVSyncTimerCallback(vsync_callback_);
  }
}

void GpuVSyncMac::SetVSyncDisplayID(int64_t display_id) {
  if (display_id == display::kInvalidDisplayId) {
    timer_based_vsync_mac_ = TimerBasedVsyncMac::GetInstance();
    DLOG(ERROR) << "GpuVSyncMac: DisplayLink Display ID is not available. "
                   "Switch to timer for GpuVSync.";
  }

  if (display_id_ == display_id) {
    return;
  }

  // Remove callback from Timer if Timer is used.
  if (!display_link_mac_ && gpu_vsync_enabled_) {
    timer_based_vsync_mac_->RemoveVSyncTimerCallback(vsync_callback_);
  }

  // Remove and unregister the old VSyncCallbackMac.
  vsync_callback_mac_ = nullptr;

  // Remove the old DisplayLinkMac.
  display_link_mac_ = nullptr;
  display_id_ = display_id;

  // Get DisplayLinkMac with the new CGDirectDisplayID.
  display_link_mac_ = ui::DisplayLinkMac::GetForDisplay(
      base::checked_cast<CGDirectDisplayID>(display_id));

  // For debugging only.
  static const bool force_timer =
      base::FeatureList::IsEnabled(kForceGpuVSyncTimerForDebugging);
  if (force_timer) {
    display_link_mac_ = nullptr;
  }

  if (display_link_mac_) {
    nominal_refresh_period_ =
        base::Seconds(1) / display_link_mac_->GetRefreshRate();
  } else {
    LOG(ERROR) << "Fail to create DisplayLinkMac for DisplayID: " << display_id
               << ". Use timer for GpuVSync";
    timer_based_vsync_mac_ = TimerBasedVsyncMac::GetInstance();
  }

  if (gpu_vsync_enabled_) {
    AddGpuVSyncCallback();
  }
}

void GpuVSyncMac::SetGpuVSyncEnabled(bool enabled) {
  if (gpu_vsync_enabled_ == enabled) {
    return;
  }
  gpu_vsync_enabled_ = enabled;

  if (enabled) {
    AddGpuVSyncCallback();
  } else {
    RemoveGpuVSyncCallback();
  }
}

void GpuVSyncMac::AddGpuVSyncCallback() {
  if (display_link_mac_) {
    DCHECK(!vsync_callback_mac_);
    // Request the callback to be called on the hihg-priority system
    // VCDisplayLink thread.
    vsync_callback_mac_ = display_link_mac_->RegisterCallback(
        base::BindRepeating(&GpuVSyncMac::OnDisplayLinkCallback,
                            weak_ptr_factory_.GetWeakPtr()),
        /*do_callback_on_register_thread=*/false);
    if (vsync_callback_mac_) {
      // RegisterCallback succeeded.
      return;
    } else {
      // Failed. Destroy DisplaylinkMac and switch to timer.
      display_link_mac_ = nullptr;
      LOG(ERROR) << "Fail to start CVDisplayLink callback for DisplayID: "
                 << display_id_ << ". Switch to timer for GpuVSync";

      timer_based_vsync_mac_ = TimerBasedVsyncMac::GetInstance();
    }
  }

  // Use timer.
  timer_based_vsync_mac_->AddVSyncTimerCallback(vsync_callback_);
}

void GpuVSyncMac::RemoveGpuVSyncCallback() {
  if (display_link_mac_) {
    DCHECK(vsync_callback_mac_);
    // Remove and unregister VSyncCallbackMac.
    vsync_callback_mac_ = nullptr;
    return;
  }
  // The timer is in use.
  timer_based_vsync_mac_->RemoveVSyncTimerCallback(vsync_callback_);
}

// Called on a high priority CVDisplayLink thread.
void GpuVSyncMac::OnDisplayLinkCallback(ui::VSyncParamsMac params) {
  base::TimeTicks timebase;
  base::TimeDelta interval;

  if (params.callback_times_valid) {
    DCHECK(params.callback_timebase != base::TimeTicks());
    DCHECK(!params.callback_interval.is_zero());
    timebase = params.callback_timebase;
    interval = params.callback_interval;
  } else {
    // Invalid parameters should be rare. Use default refresh rate.
    timebase = base::TimeTicks::Now();
    interval = params.display_times_valid ? params.display_interval
                                          : nominal_refresh_period_;
  }
  vsync_callback_.Run(timebase, interval);
}

}  // namespace gpu
