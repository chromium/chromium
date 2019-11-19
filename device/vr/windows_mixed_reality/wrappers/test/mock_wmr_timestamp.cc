// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_timestamp.h"

namespace device {

// MockWMRTimestamp
MockWMRTimestamp::MockWMRTimestamp() {}

MockWMRTimestamp::~MockWMRTimestamp() = default;

ABI::Windows::Foundation::DateTime MockWMRTimestamp::TargetTime() const {
  return ABI::Windows::Foundation::DateTime();
}

ABI::Windows::Foundation::TimeSpan MockWMRTimestamp::PredictionAmount() const {
  ABI::Windows::Foundation::TimeSpan ret;
  ret.Duration = 0;
  return ret;
}

ABI::Windows::Perception::IPerceptionTimestamp* MockWMRTimestamp::GetRawPtr()
    const {
  return nullptr;
}

}  // namespace device
