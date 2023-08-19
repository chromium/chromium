// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_NULL_VIDEO_SINK_H_
#define MEDIA_BASE_NULL_VIDEO_SINK_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_renderer_sink.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MEDIA_EXPORT NullVideoSink : public VideoRendererSink {
 public:
  using NewFrameCB = base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>;

  // Periodically calls |callback| every |interval| on |task_runner| once the
  // sink has been started.  If |clockless| is true, the RenderCallback will
  // be called back to back by repeated post tasks. Optionally, if specified,
  // |new_frame_cb| will be called for each new frame received.
  NullVideoSink(bool clockless,
                base::TimeDelta interval,
                const NewFrameCB& new_frame_cb,
                const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  NullVideoSink(const NullVideoSink&) = delete;
  NullVideoSink& operator=(const NullVideoSink&) = delete;

  ~NullVideoSink() override;

  // VideoRendererSink implementation.
  void Start(RenderCallback* callback) override;
  void Stop() override;
  void PaintSingleFrame(scoped_refptr<VideoFrame> frame,
                        bool repaint_duplicate_frame) override;

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Sets |stop_cb_|, which will be fired when Stop() is called.
  void set_stop_cb(base::OnceClosure stop_cb) { stop_cb_ = std::move(stop_cb); }

  bool is_started() const { return started_; }

  void set_background_render(bool is_background_rendering) {
    background_render_ = is_background_rendering;
  }

  void set_clockless(bool clockless) { clockless_ = clockless; }

 private:
  // Task that periodically calls Render() to consume video data.
  void CallRender();

  bool clockless_;
  const base::TimeDelta interval_;
  const NewFrameCB new_frame_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool started_;
  raw_ptr<RenderCallback, AcrossTasksDanglingUntriaged> callback_;

  // Manages cancellation of periodic Render() callback task.
  base::CancelableRepeatingClosure cancelable_worker_;

  // Used to determine when a new frame is received.
  scoped_refptr<VideoFrame> last_frame_;

  // Used to determine the interval given to RenderCallback::Render() as well as
  // to maintain stable periodicity of callbacks.
  base::TimeTicks current_render_time_;

  // Allow for an injectable tick clock for testing.
  base::TimeTicks last_now_;

  // If specified, used instead of a DefaultTickClock.
  raw_ptr<const base::TickClock> tick_clock_;

  // If set, called when Stop() is called.
  base::OnceClosure stop_cb_;

  // Value passed to RenderCallback::Render().
  bool background_render_;
};

}  // namespace media

#endif  // MEDIA_BASE_NULL_VIDEO_SINK_H_
