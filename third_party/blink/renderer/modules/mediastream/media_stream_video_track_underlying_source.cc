// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
namespace blink {

// Temporary workaround for crbug.com/1182497.
// Doesn't perform any stream optimization, but instead
// lets MediaStreamVideoTrackUnderlyingSource know that
// its stream endpoint has been transferred, and that it should mark its video
// frames for closure when they are cloned().
class StreamTransferNotifier final
    : public ReadableStreamTransferringOptimizer {
  USING_FAST_MALLOC(StreamTransferNotifier);
  using OptimizerCallback = CrossThreadOnceClosure;

 public:
  StreamTransferNotifier(
      scoped_refptr<base::SequencedTaskRunner> original_runner,
      OptimizerCallback callback)
      : original_runner_(std::move(original_runner)),
        callback_(std::move(callback)) {}

  UnderlyingSourceBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    // Send a message back to MediaStreamVideoTrackUnderlyingSource.
    PostCrossThreadTask(*original_runner_, FROM_HERE, std::move(callback_));

    // Return nullptr will mean that no optimization was performed, and streams
    // will post internally.
    return nullptr;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> original_runner_;
  OptimizerCallback callback_;
};

MediaStreamVideoTrackUnderlyingSource::MediaStreamVideoTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    wtf_size_t max_queue_size)
    : FrameQueueUnderlyingSource(script_state, max_queue_size), track_(track) {
  DCHECK(track_);
}

void MediaStreamVideoTrackUnderlyingSource::Trace(Visitor* visitor) const {
  FrameQueueUnderlyingSource::Trace(visitor);
  visitor->Trace(track_);
}

std::unique_ptr<ReadableStreamTransferringOptimizer>
MediaStreamVideoTrackUnderlyingSource::GetStreamTransferOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto stream_transferred_cb = [](MediaStreamVideoTrackUnderlyingSource* self) {
    if (self)
      self->SetStreamWasTransferred();
  };

  return std::make_unique<StreamTransferNotifier>(
      Thread::Current()->GetTaskRunner(),
      CrossThreadBindOnce(stream_transferred_cb,
                          WrapCrossThreadWeakPersistent(this)));
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    std::vector<scoped_refptr<media::VideoFrame>> /*scaled_media_frames*/,
    base::TimeTicks estimated_capture_time) {
  // The scaled video frames are currently ignored.
  QueueFrame(std::move(media_frame));
}

bool MediaStreamVideoTrackUnderlyingSource::StartFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(track_);
  if (!video_track)
    return false;

  ConnectToTrack(WebMediaStreamTrack(track_),
                 ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                     &MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack,
                     WrapCrossThreadPersistent(this))),
                 /*is_sink_secure=*/false);
  return true;
}

void MediaStreamVideoTrackUnderlyingSource::StopFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisconnectFromTrack();
}

}  // namespace blink
