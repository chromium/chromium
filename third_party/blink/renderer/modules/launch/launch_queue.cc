// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/launch_queue.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_launch_consumer.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"

namespace blink {

LaunchQueue::LaunchQueue() = default;

LaunchQueue::~LaunchQueue() = default;

void LaunchQueue::Enqueue(LaunchParams* params) {
  if (!consumer_) {
    unconsumed_launch_params_.push_back(params);
    return;
  }

  MeasureLatencyFromBrowserProcess(
      params->time_navigation_started_in_browser());
  consumer_->InvokeAndReportException(nullptr, params);
}

void LaunchQueue::setConsumer(V8LaunchConsumer* consumer) {
  consumer_ = consumer;

  // Consume all launch params now we have a consumer.
  while (!unconsumed_launch_params_.empty()) {
    // Get the first launch params and the queue and remove it before invoking
    // the consumer, in case the consumer calls |setConsumer|. Each launchParams
    // should be consumed by the most recently set consumer.
    LaunchParams* params = unconsumed_launch_params_.at(0);
    unconsumed_launch_params_.EraseAt(0);

    MeasureLatencyFromBrowserProcess(
        params->time_navigation_started_in_browser());
    consumer_->InvokeAndReportException(nullptr, params);
  }
}

void LaunchQueue::Trace(Visitor* visitor) const {
  visitor->Trace(unconsumed_launch_params_);
  visitor->Trace(consumer_);
  ScriptWrappable::Trace(visitor);
}

void LaunchQueue::MeasureLatencyFromBrowserProcess(
    const base::TimeTicks time_navigation_started_in_browser) {
  if (time_navigation_started_in_browser.is_null()) {
    return;
  }

  // Measure how long it took the launch params to be enqueued after the browser
  // process receives the navigation request that created these params.
  base::UmaHistogramMediumTimes(
      "WebApp.NavigationCapturing.LaunchParamsConsumedTime",
      base::TimeTicks::Now() - time_navigation_started_in_browser);
}

}  // namespace blink
