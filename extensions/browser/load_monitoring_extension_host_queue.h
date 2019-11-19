// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LOAD_MONITORING_EXTENSION_HOST_QUEUE_H_
#define EXTENSIONS_BROWSER_LOAD_MONITORING_EXTENSION_HOST_QUEUE_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "extensions/browser/deferred_start_render_host_observer.h"
#include "extensions/browser/extension_host_queue.h"

namespace extensions {

// An ExtensionHostQueue which just monitors, and later reports, how many
// ExtensionHosts are being loaded for some period of time.
class LoadMonitoringExtensionHostQueue
    : public ExtensionHostQueue,
      public DeferredStartRenderHostObserver {
 public:
  // Construction for testing.
  // Allows overriding the default timeout and triggering a callback when
  // monitoring has finished (timeout has elapsed and UMA is logged).
  using FinishedCallback = base::Callback<void(size_t,  // num_queued
                                               size_t,  // max_loaded
                                               size_t,  // max_awaiting_loading
                                               size_t   // max_active_loading
                                               )>;
  LoadMonitoringExtensionHostQueue(std::unique_ptr<ExtensionHostQueue> delegate,
                                   base::TimeDelta monitor_time,
                                   const FinishedCallback& finished_callback);

  // Production code should use this constructor.
  //
  // Monitoring will not start until the first Add()ed
  // DeferredStartRenderHost starts loading, or StartMonitoring() is called.
  explicit LoadMonitoringExtensionHostQueue(
      std::unique_ptr<ExtensionHostQueue> delegate);

  ~LoadMonitoringExtensionHostQueue() override;

  // Starts monitoring.
  //
  // This can be called multiple times, but it has no effect if monitoring has
  // already started (or finished). Monitoring cannot be restarted.
  //
  // Note that monitoring will automatically start when Add() is called, so it
  // may not be necessary to call this at all.
  void StartMonitoring();

  // ExtensionHostQueue:
  void Add(DeferredStartRenderHost* host) override;
  void Remove(DeferredStartRenderHost* host) override;

  // DeferredStartRenderHostObserver, public to be triggered by tests:
  void OnDeferredStartRenderHostDidStartFirstLoad(
      const DeferredStartRenderHost* host) override;
  void OnDeferredStartRenderHostDidStopFirstLoad(
      const DeferredStartRenderHost* host) override;
  void OnDeferredStartRenderHostDestroyed(
      const DeferredStartRenderHost* host) override;

 private:
  // Starts/finishes monitoring |host|, though either will have no effect if
  // monitoring has already finished.
  void StartMonitoringHost(const DeferredStartRenderHost* host);
  void FinishMonitoringHost(const DeferredStartRenderHost* host);

  // Called when monitoring should finish. Metrics are recorded, and from this
  // point on no monitoring will take place.
  void FinishMonitoring();

  // Delegate actually loading DeferredStartRenderHosts to another queue.
  std::unique_ptr<ExtensionHostQueue> delegate_;

  // The amount of time to monitor for. By default this is 1 minute, but it can
  // be overriden by tests.
  base::TimeDelta monitor_time_;

  // A callback to run when monitoring has finished. Intended for testing.
  FinishedCallback finished_callback_;

  // The hosts which are waiting to start loading.
  std::set<const DeferredStartRenderHost*> awaiting_loading_;
  // The hosts which are currently loading.
  std::set<const DeferredStartRenderHost*> active_loading_;

  // True if this has started monitoring.
  bool started_;

  // Metrics:
  // The total number of hosts that were added to the queue.
  size_t num_queued_;
  // The total number of hosts that started loading.
  size_t num_loaded_;
  // The maximum number of hosts waiting to load at the same time.
  size_t max_awaiting_loading_;
  // The maximum number of hosts that were loading at the same time.
  size_t max_active_loading_;

  base::WeakPtrFactory<LoadMonitoringExtensionHostQueue> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(LoadMonitoringExtensionHostQueue);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LOAD_MONITORING_EXTENSION_HOST_QUEUE_H_
