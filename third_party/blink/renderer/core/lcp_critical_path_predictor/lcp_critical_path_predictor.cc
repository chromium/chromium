// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

LCPCriticalPathPredictor::LCPCriticalPathPredictor(LocalFrame& frame)
    : frame_(&frame) {}

LCPCriticalPathPredictor::~LCPCriticalPathPredictor() = default;

void LCPCriticalPathPredictor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
