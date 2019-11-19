// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_

#include <windows.perception.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRTimestamp {
 public:
  virtual ~WMRTimestamp() = default;

  virtual ABI::Windows::Foundation::DateTime TargetTime() const = 0;
  virtual ABI::Windows::Foundation::TimeSpan PredictionAmount() const = 0;
  // No default implementation w/ a NOTREACHED like other GetRawPtr()s because
  // this is expected to be called on both the real and mock implementations in
  // MixedRealityInputHelper::GetInputState().
  virtual ABI::Windows::Perception::IPerceptionTimestamp* GetRawPtr() const = 0;
};

class WMRTimestampImpl : public WMRTimestamp {
 public:
  explicit WMRTimestampImpl(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp);
  ~WMRTimestampImpl() override;

  ABI::Windows::Foundation::DateTime TargetTime() const override;
  ABI::Windows::Foundation::TimeSpan PredictionAmount() const override;
  ABI::Windows::Perception::IPerceptionTimestamp* GetRawPtr() const override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
      timestamp_;

  DISALLOW_COPY_AND_ASSIGN(WMRTimestampImpl);
};

}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_
