// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace blink {

FrameVisualProperties::FrameVisualProperties() = default;

FrameVisualProperties::FrameVisualProperties(
    const FrameVisualProperties& other) = default;

FrameVisualProperties::~FrameVisualProperties() = default;

FrameVisualProperties& FrameVisualProperties::operator=(
    const FrameVisualProperties& other) = default;

}  // namespace blink
