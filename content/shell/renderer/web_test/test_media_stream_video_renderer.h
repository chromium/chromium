// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_VIDEO_RENDERER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_VIDEO_RENDERER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_video_renderer.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// A blink::WebMediaStreamVideoRenderer that generates raw frames and
// passes them to webmediaplayer.
// Since non-black pixel values are required in the web test, e.g.,
// media/video-capture-canvas.html, this class should generate frame with
// only non-black pixels.
class TestMediaStreamVideoRenderer : public blink::WebMediaStreamVideoRenderer {
 public:
  TestMediaStreamVideoRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const gfx::Size& size,
      const base::TimeDelta& frame_duration,
      const RepaintCB& repaint_cb);

  // blink::WebMediaStreamVideoRenderer implementation.
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Pause() override;

 protected:
  ~TestMediaStreamVideoRenderer() override;

 private:
  enum State {
    kStarted,
    kPaused,
    kStopped,
  };

  void GenerateFrame();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  gfx::Size size_;
  State state_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;
  RepaintCB repaint_cb_;

  DISALLOW_COPY_AND_ASSIGN(TestMediaStreamVideoRenderer);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_VIDEO_RENDERER_H_
