// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SWAP_METRICS_DRIVER_H_
#define CONTENT_PUBLIC_BROWSER_SWAP_METRICS_DRIVER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// This class collects metrics about the system's swapping behavior and provides
// these metrics to an associated delegate. Metrics can be platform-specific.
//
// Updated swap metrics can be obtained through Delegate methods in either a
// push-based manner, driven by a periodic base::Timer, or in a synchronous,
// pull-based manner. For receiving periodic updates at a regular frequency, use
// Start() to begin recieiving updates, and Stop() to end them. For obtaining
// updates at specified times, for example over a specific interval or event,
// use InitializeMetrics() at the start, and UpdateMetrics() at any subsequent
// time to immediately and synchronously receive updates. In the case of
// periodic updates, UpdateMetrics() can also be called after Stop() to retrieve
// updated metrics since the last update.
//
// The SwapMetricsDriver API is not thread safe. The Delegate methods run on
// either the sequence on which Start() was called, in the case of periodic
// updates, or the sequence InitializeMetrics() is called on in the case of
// using pull-based updates. In either case, metrics must always be updated on
// the same sequence and subsequent invocations of this API's methods must be
// made on that sequence.
class CONTENT_EXPORT SwapMetricsDriver {
 public:
  enum class SwapMetricsUpdateResult {
    kSwapMetricsUpdateSuccess,
    kSwapMetricsUpdateFailed,
  };

  // Delegate class that handles the metrics computed by SwapMetricsDriver.
  class CONTENT_EXPORT Delegate {
   public:
    virtual ~Delegate();

    virtual void OnSwapInCount(uint64_t count, base::TimeDelta interval) {}
    virtual void OnSwapOutCount(uint64_t count, base::TimeDelta interval) {}
    virtual void OnDecompressedPageCount(uint64_t count,
                                         base::TimeDelta interval) {}
    virtual void OnCompressedPageCount(uint64_t count,
                                       base::TimeDelta interval) {}
    virtual void OnUpdateMetricsFailed() {}

   protected:
    Delegate();

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  virtual ~SwapMetricsDriver() {}

  // Create a SwapMetricsDriver that will notify the |delegate| when updated
  // metrics are available to consume. If |update_interval| is 0, periodic
  // updating is disabled and metrics should be updated manually via
  // UpdateMetrics().
  // This returns nullptr when swap metrics are not available on the system.
  static std::unique_ptr<SwapMetricsDriver> Create(
      std::unique_ptr<Delegate> delegate,
      const base::TimeDelta update_interval);

  // Initialze swap metrics so updates will start from the values read from this
  // point. If an error occurs while retrieving the platform specific metrics,
  // e.g. I/O error, this returns
  // SwapMetricsUpdateResult::kSwapMetricsUpdateFailed.
  // After InitializeMetrics() is called, all subsequent calls to this API must
  // be made on the sequence this is called from.
  virtual SwapMetricsUpdateResult InitializeMetrics() = 0;

  // Returns whether or not the driver is periodically computing metrics.
  // This returns false if the driver has been stopped or never started, or if
  // an error occurred during UpdateMetrics().
  // This method must be called from the same sequence that Start() is called
  // on. If Start() has not yet been called, the subsequent call to Start() must
  // be from the same sequence.
  virtual bool IsRunning() = 0;

  // Starts computing swap metrics every at the interval specified in the
  // constructor, which must be > 0. If an error occurs while initializing the
  // metrics, this returns SwapMetricsUpdateResult::kSwapMetricsUpdateFailed. If
  // an error occurs in UpdateMetrics() during a periodic update, the driver
  // will be stopped. IsRunning() can be used to determine if metrics are still
  // being computed, or by handling the Delegate's OnMetricsFailed() method.
  // After Start() is called, all subsequent calls to this API must be made on
  // the sequence the driver is started from.
  virtual SwapMetricsUpdateResult Start() = 0;

  // Stop computing swap metrics. To compute metrics for the remaining time
  // since the last update, UpdateMetrics() can be called after Stop().
  // Stop() must be called on the same sequence as Start().
  virtual void Stop() = 0;

  // Update metrics immediately and synchronously notify the associated
  // Delegate. InitializeMetrics() or Start() must be called before
  // UpdateMetrics(), and UpdateMetrics() must be called on the same sequence
  // that those methods were invoked on. If an error occurs while retrieving the
  // platform specific metrics, e.g. I/O error, this returns
  // SwapMetricsUpdateResult::kSwapMetricsUpdateFailed and the associated
  // delegate's OnUpdateMetricsFailed() method will be called.
  virtual SwapMetricsUpdateResult UpdateMetrics() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class SwapMetricsDriverImpl;

  SwapMetricsDriver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SWAP_METRICS_DRIVER_H_
