// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_type.h"

#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace content {

perfetto::protos::pbzero::FrameTreeNodeInfo::FrameType FrameTypeToProto(
    FrameType frame_type) {
  using RFHProto = perfetto::protos::pbzero::FrameTreeNodeInfo;
  switch (frame_type) {
    case FrameType::kSubframe:
      return RFHProto::SUBFRAME;
    case FrameType::kPrimaryMainFrame:
      return RFHProto::PRIMARY_MAIN_FRAME;
    case FrameType::kPrerenderMainFrame:
      return RFHProto::PRERENDER_MAIN_FRAME;
    case FrameType::kFencedFrameRoot:
      return RFHProto::FENCED_FRAME_ROOT;
  }

  return RFHProto::UNSPECIFIED_FRAME_TYPE;
}

}  // namespace content
