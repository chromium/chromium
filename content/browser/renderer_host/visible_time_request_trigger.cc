// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace content {

VisibleTimeRequestTrigger::VisibleTimeRequestTrigger() = default;

VisibleTimeRequestTrigger::~VisibleTimeRequestTrigger() = default;

void VisibleTimeRequestTrigger::UpdateRequest(
    blink::VisibleTimeEvent new_event) {
  if (!last_request_) {
    last_request_.emplace();
  }
  last_request_->events.push_back(std::move(new_event));
}

std::optional<blink::RecordContentToVisibleTimeRequest>
VisibleTimeRequestTrigger::TakeRequest() {
  return std::exchange(last_request_, std::nullopt);
}

}  // namespace content
