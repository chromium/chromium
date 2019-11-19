// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/launch/launch_queue.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_launch_consumer.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

LaunchQueue::LaunchQueue() = default;

LaunchQueue::~LaunchQueue() = default;

void LaunchQueue::Enqueue(LaunchParams* params) {
  if (!consumer_) {
    unconsumed_launch_params_.push_back(params);
    return;
  }

  consumer_->Invoke(nullptr, params).Check();
}

void LaunchQueue::setConsumer(V8LaunchConsumer* consumer) {
  consumer_ = consumer;

  // Consume all launch params now we have a consumer.
  while (!unconsumed_launch_params_.IsEmpty()) {
    // Get the first launch params and the queue and remove it before invoking
    // the consumer, in case the consumer calls |setConsumer|. Each launchParams
    // should be consumed by the most recently set consumer.
    LaunchParams* params = unconsumed_launch_params_.at(0);
    unconsumed_launch_params_.EraseAt(0);

    consumer_->Invoke(nullptr, params).Check();
  }
}

void LaunchQueue::Trace(Visitor* visitor) {
  visitor->Trace(unconsumed_launch_params_);
  visitor->Trace(consumer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
