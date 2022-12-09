// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_REPORTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_REPORTING_H_

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_reporting.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This is the variant of `WebFencedFrameReporting` that has Blink types
// suitable for exposure to the web platform. See that struct as well as
// `blink::FencedFrame::FencedFrameReporting`.

struct FencedFrameReporting {
  HashMap<FencedFrame::ReportingDestination, HashMap<String, KURL>> metadata;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_REPORTING_H_
