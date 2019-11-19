// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_TIMESTAMP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_TIMESTAMP_H_

#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"

namespace device {

class MockWMRTimestamp : public WMRTimestamp {
 public:
  MockWMRTimestamp();
  ~MockWMRTimestamp() override;

  ABI::Windows::Foundation::DateTime TargetTime() const override;
  ABI::Windows::Foundation::TimeSpan PredictionAmount() const override;
  ABI::Windows::Perception::IPerceptionTimestamp* GetRawPtr() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRTimestamp);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_TIMESTAMP_H_
