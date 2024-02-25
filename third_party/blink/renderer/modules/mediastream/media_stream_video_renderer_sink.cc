// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_renderer_sink.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

const int kMinFrameSize = 2;

namespace blink {

// FrameDeliverer is responsible for delivering frames received on
// OnVideoFrame() to |repaint_cb_| on the video task runner.
//
// It is created on the main thread, but methods should be called and class
// should be destructed on the video task runner.
class MediaStreamVideoRendererSink::FrameDeliverer {
 public:
  FrameDeliverer(
      const RepaintCB& repaint_cb,
      base::WeakPtr<MediaStreamVideoRendererSink>
          media_stream_video_renderer_sink,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      : main_render_task_runner_(std::move(main_render_task_runner)),
        repaint_cb_(repaint_cb),
        media_stream_video_renderer_sink_(media_stream_video_renderer_sink),
        state_(kStopped),
        frame_size_(kMinFrameSize, kMinFrameSize),
        emit_frame_drop_events_(true) {
    DETACH_FROM_SEQUENCE(video_sequence_checker_);
  }

  FrameDeliverer(const FrameDeliverer&) = delete;
  FrameDeliverer& operator=(const FrameDeliverer&) = delete;

  ~FrameDeliverer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    DCHECK(state_ == kStarted || state_ == kPaused) << state_;
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks /*current_time*/) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    DCHECK(frame);
    TRACE_EVENT_INSTANT1("webrtc",
                         "MediaStreamVideoRendererSink::"
                         "FrameDeliverer::OnVideoFrame",
                         TRACE_EVENT_SCOPE_THREAD, "timestamp",
                         frame->timestamp().InMilliseconds());

    if (state_ != kStarted) {
      if (emit_frame_drop_events_) {
        emit_frame_drop_events_ = false;
        PostCrossThreadTask(
            *main_render_task_runner_, FROM_HERE,
            CrossThreadBindOnce(&MediaStreamVideoRendererSink::OnFrameDropped,
                                media_stream_video_renderer_sink_,
                                media::VideoCaptureFrameDropReason::
                                    kRendererSinkFrameDelivererIsNotStarted));
      }
      return;
    }

    frame_size_ = frame->natural_size();
    repaint_cb_.Run(std::move(frame));
  }

  void RenderEndOfStream() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    // This is necessary to make sure audio can play if the video tag src is a
    // MediaStream video track that has been rejected or ended. It also ensure
    // that the renderer doesn't hold a reference to a real video frame if no
    // more frames are provided. This is since there might be a finite number
    // of available buffers. E.g, video that originates from a video camera.
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(
            state_ == kStopped ? gfx::Size(kMinFrameSize, kMinFrameSize)
                               : frame_size_);
    if (!video_frame)
      return;

    video_frame->metadata().end_of_stream = true;
    video_frame->metadata().reference_time = base::TimeTicks::Now();
    OnVideoFrame(video_frame, base::TimeTicks());
  }

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    DCHECK_EQ(state_, kStopped);
    SetState(kStarted);
  }

  void Resume() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    if (state_ == kPaused)
      SetState(kStarted);
  }

  void Pause() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
    if (state_ == kStarted)
      SetState(kPaused);
  }

 private:
  void SetState(State target_state) {
    state_ = target_state;
    emit_frame_drop_events_ = true;
  }

  friend class MediaStreamVideoRendererSink;

  const scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;
  const RepaintCB repaint_cb_;
  base::WeakPtr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;
  State state_;
  gfx::Size frame_size_;
  bool emit_frame_drop_events_;

  // Used for DCHECKs to ensure method calls are executed on the correct thread.
  SEQUENCE_CHECKER(video_sequence_checker_);
};

MediaStreamVideoRendererSink::MediaStreamVideoRendererSink(
    MediaStreamComponent* video_component,
    const RepaintCB& repaint_cb,
    scoped_refptr<base::SequencedTaskRunner> video_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
    : repaint_cb_(repaint_cb),
      video_component_(video_component),
      video_task_runner_(std::move(video_task_runner)),
      main_render_task_runner_(std::move(main_render_task_runner)) {}

MediaStreamVideoRendererSink::~MediaStreamVideoRendererSink() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
}

void MediaStreamVideoRendererSink::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  frame_deliverer_ =
      std::make_unique<MediaStreamVideoRendererSink::FrameDeliverer>(
          repaint_cb_, weak_factory_.GetWeakPtr(), main_render_task_runner_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::Start,
                          WTF::CrossThreadUnretained(frame_deliverer_.get())));

  auto uses_alpha =
      base::FeatureList::IsEnabled(features::kAllowDropAlphaForMediaStream)
          ? MediaStreamVideoSink::UsesAlpha::kDependsOnOtherSinks
          : MediaStreamVideoSink::UsesAlpha::kDefault;

  MediaStreamVideoSink::ConnectToTrack(
      WebMediaStreamTrack(video_component_.Get()),
      // This callback is run on video task runner. It is safe to use
      // base::Unretained here because |frame_receiver_| will be destroyed on
      // video task runner after sink is disconnected from track.
      ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          &FrameDeliverer::OnVideoFrame,
          WTF::CrossThreadUnretained(frame_deliverer_.get()))),
      // Local display video rendering is considered a secure link.
      MediaStreamVideoSink::IsSecure::kYes, uses_alpha);

  if (video_component_->GetReadyState() ==
          MediaStreamSource::kReadyStateEnded ||
      !video_component_->Enabled()) {
    PostCrossThreadTask(
        *video_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&FrameDeliverer::RenderEndOfStream,
                            CrossThreadUnretained(frame_deliverer_.get())));
  }
}

void MediaStreamVideoRendererSink::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  MediaStreamVideoSink::DisconnectFromTrack();
  if (frame_deliverer_)
    video_task_runner_->DeleteSoon(FROM_HERE, frame_deliverer_.release());
}

void MediaStreamVideoRendererSink::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return;

  PostCrossThreadTask(*video_task_runner_, FROM_HERE,
                      WTF::CrossThreadBindOnce(
                          &FrameDeliverer::Resume,
                          WTF::CrossThreadUnretained(frame_deliverer_.get())));
}

void MediaStreamVideoRendererSink::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return;

  PostCrossThreadTask(*video_task_runner_, FROM_HERE,
                      WTF::CrossThreadBindOnce(
                          &FrameDeliverer::Pause,
                          WTF::CrossThreadUnretained(frame_deliverer_.get())));
}

void MediaStreamVideoRendererSink::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (state == WebMediaStreamSource::kReadyStateEnded && frame_deliverer_) {
    PostCrossThreadTask(
        *video_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &FrameDeliverer::RenderEndOfStream,
            WTF::CrossThreadUnretained(frame_deliverer_.get())));
  }
}

MediaStreamVideoRendererSink::State
MediaStreamVideoRendererSink::GetStateForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return kStopped;
  return frame_deliverer_->state_;
}

}  // namespace blink
