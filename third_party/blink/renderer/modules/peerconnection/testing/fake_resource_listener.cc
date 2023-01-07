// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/testing/fake_resource_listener.h"

#include "base/check.h"

namespace blink {

size_t FakeResourceListener::measurement_count() const {
  return measurement_count_;
}

webrtc::ResourceUsageState FakeResourceListener::latest_measurement() const {
  DCHECK(measurement_count_);
  return latest_measurement_;
}

void FakeResourceListener::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<webrtc::Resource> resource,
    webrtc::ResourceUsageState usage_state) {
  latest_measurement_ = usage_state;
  ++measurement_count_;
}

}  // namespace blink
