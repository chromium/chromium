// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/test_media_stream_renderer_factory.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "content/shell/renderer/web_test/test_media_stream_video_renderer.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFrameDurationMs = 33;

bool IsMockMediaStreamWithVideo(const blink::WebMediaStream& web_stream) {
  if (web_stream.IsNull())
    return false;
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
      web_stream.VideoTracks();
  return video_tracks.size() > 0;
}

}  // namespace

namespace content {

TestMediaStreamRendererFactory::TestMediaStreamRendererFactory() {}

TestMediaStreamRendererFactory::~TestMediaStreamRendererFactory() {}

scoped_refptr<blink::WebMediaStreamVideoRenderer>
TestMediaStreamRendererFactory::GetVideoRenderer(
    const blink::WebMediaStream& web_stream,
    const blink::WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner) {
  if (!IsMockMediaStreamWithVideo(web_stream))
    return nullptr;

  return new TestMediaStreamVideoRenderer(
      std::move(io_task_runner),
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs),
      repaint_cb);
}

scoped_refptr<blink::WebMediaStreamAudioRenderer>
TestMediaStreamRendererFactory::GetAudioRenderer(
    const blink::WebMediaStream& web_stream,
    blink::WebLocalFrame* web_frame,
    const blink::WebString& device_id) {
  return nullptr;
}

}  // namespace content
