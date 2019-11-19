// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/load_monitoring_extension_host_queue.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/serial_extension_host_queue.h"

namespace extensions {

LoadMonitoringExtensionHostQueue::LoadMonitoringExtensionHostQueue(
    std::unique_ptr<ExtensionHostQueue> delegate,
    base::TimeDelta monitor_time,
    const FinishedCallback& finished_callback)
    : delegate_(std::move(delegate)),
      monitor_time_(monitor_time),
      finished_callback_(finished_callback),
      started_(false),
      num_queued_(0u),
      num_loaded_(0u),
      max_awaiting_loading_(0u),
      max_active_loading_(0u) {}

LoadMonitoringExtensionHostQueue::LoadMonitoringExtensionHostQueue(
    std::unique_ptr<ExtensionHostQueue> delegate)
    : LoadMonitoringExtensionHostQueue(std::move(delegate),
                                       base::TimeDelta::FromMinutes(1),
                                       FinishedCallback()) {}

LoadMonitoringExtensionHostQueue::~LoadMonitoringExtensionHostQueue() {
}

void LoadMonitoringExtensionHostQueue::StartMonitoring() {
  if (started_) {
    return;
  }
  started_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LoadMonitoringExtensionHostQueue::FinishMonitoring,
                     weak_ptr_factory_.GetWeakPtr()),
      monitor_time_);
}

void LoadMonitoringExtensionHostQueue::Add(DeferredStartRenderHost* host) {
  StartMonitoring();
  delegate_->Add(host);
  host->AddDeferredStartRenderHostObserver(this);
  if (awaiting_loading_.insert(host).second) {
    ++num_queued_;
    max_awaiting_loading_ =
        std::max(max_awaiting_loading_, awaiting_loading_.size());
  }
}

void LoadMonitoringExtensionHostQueue::Remove(DeferredStartRenderHost* host) {
  delegate_->Remove(host);
  host->RemoveDeferredStartRenderHostObserver(this);
}

void LoadMonitoringExtensionHostQueue::
    OnDeferredStartRenderHostDidStartFirstLoad(
        const DeferredStartRenderHost* host) {
  StartMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::
    OnDeferredStartRenderHostDidStopFirstLoad(
        const DeferredStartRenderHost* host) {
  FinishMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::OnDeferredStartRenderHostDestroyed(
    const DeferredStartRenderHost* host) {
  FinishMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::StartMonitoringHost(
    const DeferredStartRenderHost* host) {
  awaiting_loading_.erase(host);
  if (active_loading_.insert(host).second) {
    max_active_loading_ = std::max(max_active_loading_, active_loading_.size());
  }
}

void LoadMonitoringExtensionHostQueue::FinishMonitoringHost(
    const DeferredStartRenderHost* host) {
  if (active_loading_.erase(host)) {
    ++num_loaded_;
  }
}

void LoadMonitoringExtensionHostQueue::FinishMonitoring() {
  CHECK(started_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.NumQueued",
                           num_queued_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.NumLoaded",
                           num_loaded_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.MaxInQueue",
                           max_awaiting_loading_);
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ExtensionHostMonitoring.MaxActiveLoading",
      max_active_loading_);
  if (!finished_callback_.is_null()) {
    finished_callback_.Run(num_queued_, num_loaded_, max_awaiting_loading_,
                           max_active_loading_);
  }
}

}  // namespace extensions
