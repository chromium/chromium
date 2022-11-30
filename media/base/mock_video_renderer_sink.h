// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_VIDEO_RENDERER_SINK_H_
#define MEDIA_BASE_MOCK_VIDEO_RENDERER_SINK_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoRendererSink : public VideoRendererSink {
 public:
  MockVideoRendererSink();
  ~MockVideoRendererSink() override;

  MOCK_METHOD1(Start, void(RenderCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(PaintSingleFrame,
               void(scoped_refptr<VideoFrame> frame,
                    bool repaint_duplicate_frame));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_VIDEO_RENDERER_SINK_H_
