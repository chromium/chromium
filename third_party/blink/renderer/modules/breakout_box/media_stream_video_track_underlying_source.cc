// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_source.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
constexpr char kScreenPrefix[] = "screen:";
constexpr char kWindowPrefix[] = "window:";

bool IsScreenOrWindowCapture(const std::string& device_id) {
  return base::StartsWith(device_id, kScreenPrefix,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(device_id, kWindowPrefix,
                          base::CompareCase::SENSITIVE);
}
}  // namespace

BASE_FEATURE(kBreakoutBoxFrameLimiter,
             "BreakoutBoxFrameLimiter",
             base::FEATURE_ENABLED_BY_DEFAULT);

const int MediaStreamVideoTrackUnderlyingSource::kMaxMonitoredFrameCount = 20;
const int MediaStreamVideoTrackUnderlyingSource::kMinMonitoredFrameCount = 2;

MediaStreamVideoTrackUnderlyingSource::MediaStreamVideoTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    ScriptWrappable* media_stream_track_processor,
    wtf_size_t max_queue_size)
    : FrameQueueUnderlyingSource(
          script_state,
          max_queue_size,
          GetDeviceIdForMonitoring(
              track->Source()->GetPlatformSource()->device()),
          GetFramePoolSize(track->Source()->GetPlatformSource()->device())),
      media_stream_track_processor_(media_stream_track_processor),
      track_(track) {
  DCHECK(track_);
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableVideo);
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
          WrapCrossThreadWeakPersistent(this)),
      CrossThreadBindOnce(
          &MediaStreamVideoTrackUnderlyingSource::ClearTransferredSource,
          WrapCrossThreadWeakPersistent(this)));
}

void MediaStreamVideoTrackUnderlyingSource::OnSourceTransferStarted(
    scoped_refptr<base::SequencedTaskRunner> transferred_runner,
    CrossThreadPersistent<TransferredVideoFrameQueueUnderlyingSource> source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransferSource(std::move(source));
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableVideoWorker);
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    base::TimeTicks estimated_capture_time) {
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

// static
std::string MediaStreamVideoTrackUnderlyingSource::GetDeviceIdForMonitoring(
    const MediaStreamDevice& device) {
  if (!base::FeatureList::IsEnabled(kBreakoutBoxFrameLimiter))
    return std::string();

  switch (device.type) {
    case mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return device.id;
    case mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      if (IsScreenOrWindowCapture(device.id))
        return device.id;
      [[fallthrough]];
    default:
      return std::string();
  }
}

// static
wtf_size_t MediaStreamVideoTrackUnderlyingSource::GetFramePoolSize(
    const MediaStreamDevice& device) {
  switch (device.type) {
    case mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return static_cast<wtf_size_t>(std::min(
          MediaStreamVideoTrackUnderlyingSource::kMaxMonitoredFrameCount,
          std::max(
              MediaStreamVideoTrackUnderlyingSource::kMinMonitoredFrameCount,
              std::max(media::kVideoCaptureDefaultMaxBufferPoolSize / 2,
                       media::DeviceVideoCaptureMaxBufferPoolSize() / 3))));
    case mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      if (IsScreenOrWindowCapture(device.id)) {
        return static_cast<wtf_size_t>(std::min(
            MediaStreamVideoTrackUnderlyingSource::kMaxMonitoredFrameCount,
            std::max(
                MediaStreamVideoTrackUnderlyingSource::kMinMonitoredFrameCount,
                media::kVideoCaptureDefaultMaxBufferPoolSize / 2)));
      }
      [[fallthrough]];
    default:
      // There will be no monitoring and no frame pool size. Return 0 to signal
      // that the returned value will not be used.
      return 0u;
  }
}

}  // namespace blink
