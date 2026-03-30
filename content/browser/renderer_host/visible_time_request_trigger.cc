// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <algorithm>
#include <utility>

namespace content {

VisibleTimeRequestTrigger::VisibleTimeRequestTrigger() = default;

VisibleTimeRequestTrigger::~VisibleTimeRequestTrigger() = default;

void VisibleTimeRequestTrigger::UpdateRequest(
    blink::RecordContentToVisibleTimeRequest new_request) {
  // If `last_request_` is null, this will return `new_request` unchanged.
  last_request_ = blink::ConsumeAndMergeContentToVisibleTimeRequests(
      std::move(last_request_), std::move(new_request));
}

std::optional<blink::RecordContentToVisibleTimeRequest>
VisibleTimeRequestTrigger::TakeRequest() {
  return std::exchange(last_request_, std::nullopt);
}

}  // namespace content
