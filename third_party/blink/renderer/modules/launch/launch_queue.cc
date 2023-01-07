// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/launch_queue.h"

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

    consumer_->InvokeAndReportException(nullptr, params);
  }
}

void LaunchQueue::Trace(Visitor* visitor) const {
  visitor->Trace(unconsumed_launch_params_);
  visitor->Trace(consumer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
