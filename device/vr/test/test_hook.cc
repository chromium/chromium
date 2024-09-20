// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/test_hook.h"

namespace device {

ControllerFrameData::ControllerFrameData() = default;
ControllerFrameData::~ControllerFrameData() = default;
ControllerFrameData::ControllerFrameData(const ControllerFrameData& other) =
    default;
ControllerFrameData& ControllerFrameData::operator=(
    const ControllerFrameData& other) = default;
ControllerFrameData& ControllerFrameData::operator=(
    ControllerFrameData&& other) = default;
}  // namespace device
