// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/frame_queue_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

namespace {

void PostFrameToTransferredSource(
    scoped_refptr<base::SequencedTaskRunner> transferred_runner,
    TransferredVideoFrameQueueUnderlyingSource* transferred_source,
    scoped_refptr<media::VideoFrame> media_frame) {
  PostCrossThreadTask(
      *transferred_runner.get(), FROM_HERE,
      CrossThreadBindOnce(
          &TransferredVideoFrameQueueUnderlyingSource::QueueFrame,
          WrapCrossThreadPersistent(transferred_source),
          std::move(media_frame)));
}

}  // namespace

MediaStreamVideoTrackUnderlyingSource::MediaStreamVideoTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    ScriptWrappable* media_stream_track_processor,
    wtf_size_t max_queue_size)
    : FrameQueueUnderlyingSource(script_state, max_queue_size),
      media_stream_track_processor_(media_stream_track_processor),
      track_(track) {
  DCHECK(track_);
}

void MediaStreamVideoTrackUnderlyingSource::Trace(Visitor* visitor) const {
  FrameQueueUnderlyingSource::Trace(visitor);
  visitor->Trace(media_stream_track_processor_);
  visitor->Trace(track_);
}

std::unique_ptr<ReadableStreamTransferringOptimizer>
MediaStreamVideoTrackUnderlyingSource::GetStreamTransferOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<VideoFrameQueueTransferOptimizer>(
      this, GetRealmRunner(), MaxQueueSize(),
      CrossThreadBindOnce(
          &MediaStreamVideoTrackUnderlyingSource::OnSourceTransferStarted,
          WrapCrossThreadWeakPersistent(this)));
}

scoped_refptr<base::SequencedTaskRunner>
MediaStreamVideoTrackUnderlyingSource::GetIOTaskRunner() {
  return Platform::Current()->GetIOTaskRunner();
}

void MediaStreamVideoTrackUnderlyingSource::OnSourceTransferStarted(
    scoped_refptr<base::SequencedTaskRunner> transferred_runner,
    TransferredVideoFrameQueueUnderlyingSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!transferred_source_);

  transferred_runner_ = std::move(transferred_runner);
  transferred_source_ = source;

  auto finalize_transfer = [](MediaStreamVideoTrackUnderlyingSource* self) {
    DCHECK(self->GetIOTaskRunner()->RunsTasksInCurrentSequence());
    self->was_transferred_ = true;
  };

  // All queued frames will be immediately transferred. All frames in flight for
  // the main thread will be immediately transferred as they arrive.
  //
  // New frames queued via QueueFrame() in OnFrameFromTrack() will be saved in
  // a temporary queue, only accessed on the IO thread, until
  // FinalizeQueueTransfer() is called.
  TransferQueueFromRealmRunner(
      CrossThreadBindRepeating(&PostFrameToTransferredSource,
                               transferred_runner_,
                               WrapCrossThreadPersistent(source)),
      GetIOTaskRunner(),
      CrossThreadBindOnce(finalize_transfer, WrapCrossThreadPersistent(this)));
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    std::vector<scoped_refptr<media::VideoFrame>> /*scaled_media_frames*/,
    base::TimeTicks estimated_capture_time) {
  DCHECK(GetIOTaskRunner()->RunsTasksInCurrentSequence());

  if (was_transferred_) {
    PostFrameToTransferredSource(transferred_runner_, transferred_source_.Get(),
                                 std::move(media_frame));
    return;
  }

  // The scaled video frames are currently ignored.
  QueueFrame(std::move(media_frame));
}

bool MediaStreamVideoTrackUnderlyingSource::StartFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connected_track())
    return true;

  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(track_);
  if (!video_track)
    return false;

  ConnectToTrack(WebMediaStreamTrack(track_),
                 ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                     &MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack,
                     WrapCrossThreadPersistent(this))),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);
  return true;
}

void MediaStreamVideoTrackUnderlyingSource::StopFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisconnectFromTrack();
}

}  // namespace blink
