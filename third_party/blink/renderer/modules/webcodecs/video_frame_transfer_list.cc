// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_transfer_list.h"

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

namespace blink {

const void* const VideoFrameTransferList::kTransferListKey = nullptr;

void VideoFrameTransferList::FinalizeTransfer(ExceptionState& exception_state) {
  for (VideoFrame* frame : video_frames)
    frame->close();
}

void VideoFrameTransferList::Trace(Visitor* visitor) const {
  visitor->Trace(video_frames);
}

}  // namespace blink
