// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"

#include <windows.perception.h>
#include <wrl.h>

#include "base/logging.h"

using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::TimeSpan;
using ABI::Windows::Perception::IPerceptionTimestamp;
using Microsoft::WRL::ComPtr;

namespace device {

WMRTimestampImpl::WMRTimestampImpl(ComPtr<IPerceptionTimestamp> timestamp)
    : timestamp_(timestamp) {
  DCHECK(timestamp_);
}

WMRTimestampImpl::~WMRTimestampImpl() = default;

DateTime WMRTimestampImpl::TargetTime() const {
  DateTime val;
  HRESULT hr = timestamp_->get_TargetTime(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

TimeSpan WMRTimestampImpl::PredictionAmount() const {
  TimeSpan val;
  HRESULT hr = timestamp_->get_PredictionAmount(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

IPerceptionTimestamp* WMRTimestampImpl::GetRawPtr() const {
  return timestamp_.Get();
}
}  // namespace device
