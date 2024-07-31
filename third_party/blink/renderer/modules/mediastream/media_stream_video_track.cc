// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// A lower-bound for the refresh interval.
constexpr base::TimeDelta kLowerBoundRefreshInterval =
    base::Hertz(media::limits::kMaxFramesPerSecond);

// This alias mimics the definition of VideoCaptureDeliverFrameCB.
using VideoCaptureDeliverFrameInternalCallback = WTF::CrossThreadFunction<void(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time)>;

// This alias mimics the definition of VideoCaptureNotifyFrameDroppedCB.
using VideoCaptureNotifyFrameDroppedInternalCallback =
    WTF::CrossThreadFunction<void(media::VideoCaptureFrameDropReason)>;

// Mimics blink::EncodedVideoFrameCB
using EncodedVideoFrameInternalCallback =
    WTF::CrossThreadFunction<void(scoped_refptr<EncodedVideoFrame> frame,
                                  base::TimeTicks estimated_capture_time)>;

base::TimeDelta ComputeRefreshIntervalFromBounds(
    const base::TimeDelta required_min_refresh_interval,
    const std::optional<double>& min_frame_rate,
    const std::optional<double>& max_frame_rate) {
  // Start with the default required refresh interval, and refine based on
  // constraints. If a minimum frameRate is provided, use that. Otherwise, use
  // the maximum frameRate if it happens to be less than the default.
  base::TimeDelta refresh_interval = required_min_refresh_interval;
  if (min_frame_rate.has_value())
    refresh_interval = base::Hertz(*min_frame_rate);

  if (max_frame_rate.has_value()) {
    refresh_interval = std::max(refresh_interval, base::Hertz(*max_frame_rate));
  }

  if (refresh_interval < kLowerBoundRefreshInterval)
    refresh_interval = kLowerBoundRefreshInterval;

  return refresh_interval;
}

void LogVideoFrameDropUMA(media::VideoCaptureFrameDropReason reason,
                          mojom::blink::MediaStreamType stream_type) {
  const int kEnumCount =
      static_cast<int>(media::VideoCaptureFrameDropReason::kMaxValue) + 1;
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.Track.FrameDrop", reason,
                            kEnumCount);
  switch (stream_type) {
    case mojom::blink::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.DeviceCapture", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.GumTabCapture", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.GumDesktopCapture", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.DisplayCapture", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.DisplayCaptureCurrentTab", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.Track.FrameDrop.DisplayCaptureSet", reason,
          kEnumCount);
      break;
    case mojom::blink::MediaStreamType::NO_SERVICE:
    case mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::NUM_MEDIA_TYPES:
      break;
  }
}

}  // namespace

// MediaStreamVideoTrack::FrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB/EncodedVideoFrameCB callbacks on the main render
// thread to receive video frames on the video task runner. Frames are only
// delivered to the sinks if the track is enabled. If the track is disabled, a
// black frame is instead forwarded to the sinks at the same frame rate. A
// disabled track does not forward data to encoded sinks.
class MediaStreamVideoTrack::FrameDeliverer
    : public WTF::ThreadSafeRefCounted<FrameDeliverer> {
 public:
  using VideoSinkId = WebMediaStreamSink*;

  FrameDeliverer(
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner,
      scoped_refptr<base::SequencedTaskRunner> video_task_runner,
      base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
      base::WeakPtr<MediaStreamVideoSource> media_stream_video_source,
      bool enabled,
      uint32_t sub_capture_target_version);

  FrameDeliverer(const FrameDeliverer&) = delete;
  FrameDeliverer& operator=(const FrameDeliverer&) = delete;

  // Sets whether the track is enabled or not. If getting enabled and encoded
  // output is enabled, the deliverer will wait until the next key frame before
  // it resumes producing encoded data.
  void SetEnabled(bool enabled, bool await_key_frame);

  // Add |callback| to receive video frames on the video task runner.
  // Must be called on the main render thread.
  void AddCallback(VideoSinkId id, VideoCaptureDeliverFrameCB callback);

  // Sets the frame dropped callback of the sink of frame |id].
  void SetNotifyFrameDroppedCallback(VideoSinkId id,
                                     VideoCaptureNotifyFrameDroppedCB callback);

  // Add |callback| to receive encoded video frames on the video task runner.
  // Must be called on the main render thread.
  void AddEncodedCallback(VideoSinkId id, EncodedVideoFrameCB callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(VideoSinkId id);

  // Removes encoded callback associated with |id| from receiving video frames
  // if |id| has been added. It is ok to call RemoveEncodedCallback even if the
  // |id| has not been added. Note that the added callback will be reset on the
  // main thread. Must be called on the main render thread.
  void RemoveEncodedCallback(VideoSinkId id);

  // Triggers all registered callbacks with |frame| and |estimated_capture_time|
  // as parameters. Must be called on the video task runner.
  void DeliverFrameOnVideoTaskRunner(
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks estimated_capture_time);

  // A frame was dropped instead of delivered. This is the main handler of frame
  // drops: it updates dropped/discarded counters, invokes
  // LogFrameDroppedOnVideoTaskRunner() and notifies `callbacks_` (i.e. sinks)
  // that a frame was dropped.
  void OnFrameDroppedOnVideoTaskRunner(
      media::VideoCaptureFrameDropReason reason);

  // Can be called from any task runner (is atomic).
  size_t deliverable_frames() const { return deliverable_frames_; }
  size_t discarded_frames() const { return discarded_frames_; }
  size_t dropped_frames() const { return dropped_frames_; }

  // Triggers all encoded callbacks with |frame| and |estimated_capture_time|.
  // Must be called on the video task runner.
  void DeliverEncodedVideoFrameOnVideoTaskRunner(
      scoped_refptr<EncodedVideoFrame> frame,
      base::TimeTicks estimated_capture_time);

  // Called when a sub-capture-target-version is acknowledged by the capture
  // module. After this, it is guaranteed that all subsequent frames will be
  // associated with a sub-capture-target-version that is >=
  // |sub_capture_target_version|. Must be called on the video task runner.
  void NewSubCaptureTargetVersionOnVideoTaskRunner(
      uint32_t sub_capture_target_version);

  void SetIsRefreshingForMinFrameRate(bool is_refreshing_for_min_frame_rate);

  void AddSubCaptureTargetVersionCallback(uint32_t sub_capture_target_version,
                                          base::OnceClosure callback);
  void RemoveSubCaptureTargetVersionCallback(
      uint32_t sub_capture_target_version);

  // Performs logging and UMAs relating to frame drops. This includes both
  // frames dropped prior to delivery (OnFrameDroppedOnVideoTaskRunner) and
  // MediaStreamVideoTrack::OnSinkDroppedFrame().
  void LogFrameDroppedOnVideoTaskRunner(
      media::VideoCaptureFrameDropReason reason);

  void SetEmitLogMessage(
      base::RepeatingCallback<void(const std::string&)> emit_log_message);

 private:
  friend class WTF::ThreadSafeRefCounted<FrameDeliverer>;

  // Struct containing sink id, frame delivery and frame dropped callbacks.
  struct VideoIdCallbacks {
    VideoSinkId id;
    VideoCaptureDeliverFrameInternalCallback deliver_frame;
    VideoCaptureNotifyFrameDroppedInternalCallback notify_frame_dropped;
  };

  virtual ~FrameDeliverer();
  void AddCallbackOnVideoTaskRunner(
      VideoSinkId id,
      VideoCaptureDeliverFrameInternalCallback callback);
  void SetNotifyFrameDroppedCallbackOnVideoTaskRunner(
      VideoSinkId id,
      VideoCaptureNotifyFrameDroppedInternalCallback callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  void RemoveCallbackOnVideoTaskRunner(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void AddEncodedCallbackOnVideoTaskRunner(
      VideoSinkId id,
      EncodedVideoFrameInternalCallback callback);
  void RemoveEncodedCallbackOnVideoTaskRunner(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void SetEnabledOnVideoTaskRunner(bool enabled, bool await_key_frame);

  void SetIsRefreshingForMinFrameRateOnVideoTaskRunner(
      bool is_refreshing_for_min_frame_rate);

  void AddSubCaptureTargetVersionCallbackOnVideoTaskRunner(
      uint32_t sub_capture_target_version,
      WTF::CrossThreadOnceClosure callback);
  void RemoveSubCaptureTargetVersionCallbackOnVideoTaskRunner(
      uint32_t sub_capture_target_version);

  // Returns a black frame where the size and time stamp is set to the same as
  // as in |reference_frame|.
  scoped_refptr<media::VideoFrame> GetBlackFrame(
      const media::VideoFrame& reference_frame);

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // Render Thread.
  THREAD_CHECKER(main_render_thread_checker_);
  const scoped_refptr<base::SequencedTaskRunner> video_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;

  base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track_;
  base::WeakPtr<MediaStreamVideoSource> media_stream_video_source_;
  const mojom::blink::MediaStreamType stream_type_;

  bool enabled_;
  scoped_refptr<media::VideoFrame> black_frame_;
  bool emit_frame_drop_events_;

  Vector<VideoIdCallbacks> callbacks_;
  HashMap<VideoSinkId, EncodedVideoFrameInternalCallback> encoded_callbacks_;

  // Frame counters for the MediaStreamTrack Statistics API. The counters are
  // only incremented when the track is enabled (even though a disabled track
  // delivers black frames).
  std::atomic<size_t> deliverable_frames_ = 0;
  std::atomic<size_t> discarded_frames_ = 0;
  std::atomic<size_t> dropped_frames_ = 0;

  // Helper methods for LogFrameDroppedOnVideoTaskRunner().
  void MaybeEmitFrameDropLogMessage(media::VideoCaptureFrameDropReason reason);
  void EmitLogMessage(const std::string& message);
  base::RepeatingCallback<void(const std::string&)> emit_log_message_;
  // States relating to frame drop logging and UMAs.
  struct FrameDropLogState {
    explicit FrameDropLogState(media::VideoCaptureFrameDropReason reason =
                                   media::VideoCaptureFrameDropReason::kNone);

    int drop_count = 0;
    media::VideoCaptureFrameDropReason drop_reason =
        media::VideoCaptureFrameDropReason::kNone;
    bool max_log_count_exceeded = false;
  };
  FrameDropLogState frame_drop_log_state_;
  // Tracks how often each frame-drop reason was encountered to decide whether
  // or not to LOG the console.
  std::map<media::VideoCaptureFrameDropReason, int> frame_drop_log_counters_
      ALLOW_DISCOURAGED_TYPE("TODO(crbug.com/1481448)");

  // Callbacks that will be invoked a single time when a
  // sub-capture-target-version is observed that is at least equal to the key.
  // The map itself (sub_capture_target_version_callbacks_) is bound to the
  // video task runner. The callbacks are bound to their respective threads
  // (BindPostTask).
  HashMap<uint32_t, WTF::CrossThreadOnceClosure>
      sub_capture_target_version_callbacks_;

  bool await_next_key_frame_;

  // This should only be accessed on the video task runner.
  bool is_refreshing_for_min_frame_rate_ = false;

  // This monotonously increasing value indicates which
  // sub-capture-target-version is expected for delivered frames.
  uint32_t sub_capture_target_version_ = 0;
};

MediaStreamVideoTrack::FrameDeliverer::FrameDropLogState::FrameDropLogState(
    media::VideoCaptureFrameDropReason reason)
    : drop_count((reason == media::VideoCaptureFrameDropReason::kNone) ? 0 : 1),
      drop_reason(reason),
      max_log_count_exceeded(false) {}

MediaStreamVideoTrack::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner,
    scoped_refptr<base::SequencedTaskRunner> video_task_runner,
    base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
    base::WeakPtr<MediaStreamVideoSource> media_stream_video_source,
    bool enabled,
    uint32_t sub_capture_target_version)
    : video_task_runner_(std::move(video_task_runner)),
      main_render_task_runner_(main_render_task_runner),
      media_stream_video_track_(media_stream_video_track),
      media_stream_video_source_(media_stream_video_source),
      stream_type_(media_stream_video_source_->device().type),
      enabled_(enabled),
      emit_frame_drop_events_(true),
      await_next_key_frame_(false),
      sub_capture_target_version_(sub_capture_target_version) {
  DCHECK(video_task_runner_.get());
  DCHECK(main_render_task_runner_);
  SetEmitLogMessage(ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &MediaStreamVideoTrack::FrameDeliverer::EmitLogMessage,
      WTF::CrossThreadUnretained(this))));
}

MediaStreamVideoTrack::FrameDeliverer::~FrameDeliverer() {
  DCHECK(callbacks_.empty());
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallback(
    VideoSinkId id,
    VideoCaptureDeliverFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::AddCallbackOnVideoTaskRunner,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          CrossThreadBindRepeating(std::move(callback))));
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallbackOnVideoTaskRunner(
    VideoSinkId id,
    VideoCaptureDeliverFrameInternalCallback callback) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  callbacks_.push_back(VideoIdCallbacks{
      id, std::move(callback),
      CrossThreadBindRepeating([](media::VideoCaptureFrameDropReason) {})});
}

void MediaStreamVideoTrack::FrameDeliverer::SetNotifyFrameDroppedCallback(
    VideoSinkId id,
    VideoCaptureNotifyFrameDroppedCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::SetNotifyFrameDroppedCallbackOnVideoTaskRunner,
          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
          CrossThreadBindRepeating(std::move(callback)),
          main_render_task_runner_));
}

void MediaStreamVideoTrack::FrameDeliverer::
    SetNotifyFrameDroppedCallbackOnVideoTaskRunner(
        VideoSinkId id,
        VideoCaptureNotifyFrameDroppedInternalCallback callback,
        const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << __func__;
  for (auto& entry : callbacks_) {
    if (entry.id == id) {
      // Old callback destruction needs to happen on the specified task
      // runner.
      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(
              [](VideoCaptureNotifyFrameDroppedInternalCallback) {},
              std::move(entry.notify_frame_dropped)));
      entry.notify_frame_dropped = std::move(callback);
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::AddEncodedCallback(
    VideoSinkId id,
    EncodedVideoFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::AddEncodedCallbackOnVideoTaskRunner,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          CrossThreadBindRepeating(std::move(callback))));
}

void MediaStreamVideoTrack::FrameDeliverer::AddEncodedCallbackOnVideoTaskRunner(
    VideoSinkId id,
    EncodedVideoFrameInternalCallback callback) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  encoded_callbacks_.insert(id, std::move(callback));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallback(VideoSinkId id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::RemoveCallbackOnVideoTaskRunner,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          main_render_task_runner_));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallbackOnVideoTaskRunner(
    VideoSinkId id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  auto it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->id == id) {
      // Callback destruction needs to happen on the specified task runner.
      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(
              [](VideoCaptureDeliverFrameInternalCallback frame,
                 VideoCaptureNotifyFrameDroppedInternalCallback dropped) {},
              std::move(it->deliver_frame),
              std::move(it->notify_frame_dropped)));
      callbacks_.erase(it);
      return;
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveEncodedCallback(
    VideoSinkId id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::RemoveEncodedCallbackOnVideoTaskRunner,
          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
          main_render_task_runner_));
}

void MediaStreamVideoTrack::FrameDeliverer::
    RemoveEncodedCallbackOnVideoTaskRunner(
        VideoSinkId id,
        const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());

  // Callback destruction needs to happen on the specified task runner.
  auto it = encoded_callbacks_.find(id);
  if (it == encoded_callbacks_.end())
    return;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce([](EncodedVideoFrameInternalCallback callback) {},
                          std::move(it->value)));
  encoded_callbacks_.erase(it);
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabled(bool enabled,
                                                       bool await_key_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::SetEnabledOnVideoTaskRunner,
                          WrapRefCounted(this), enabled, await_key_frame));
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnVideoTaskRunner(
    bool enabled,
    bool await_key_frame) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  if (enabled != enabled_) {
    enabled_ = enabled;
    emit_frame_drop_events_ = true;
  }
  if (enabled_) {
    black_frame_ = nullptr;
    await_next_key_frame_ = await_key_frame;
  }
}

void MediaStreamVideoTrack::FrameDeliverer::SetIsRefreshingForMinFrameRate(
    bool is_refreshing_for_min_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::SetIsRefreshingForMinFrameRateOnVideoTaskRunner,
          WrapRefCounted(this), is_refreshing_for_min_frame_rate));
}

void MediaStreamVideoTrack::FrameDeliverer::AddSubCaptureTargetVersionCallback(
    uint32_t sub_capture_target_version,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::AddSubCaptureTargetVersionCallbackOnVideoTaskRunner,
          WrapRefCounted(this), sub_capture_target_version,
          CrossThreadBindOnce(std::move(callback))));
}

void MediaStreamVideoTrack::FrameDeliverer::
    RemoveSubCaptureTargetVersionCallback(uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::
              RemoveSubCaptureTargetVersionCallbackOnVideoTaskRunner,
          WrapRefCounted(this), sub_capture_target_version));
}

void MediaStreamVideoTrack::FrameDeliverer::
    SetIsRefreshingForMinFrameRateOnVideoTaskRunner(
        bool is_refreshing_for_min_frame_rate) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  is_refreshing_for_min_frame_rate_ = is_refreshing_for_min_frame_rate;
}

void MediaStreamVideoTrack::FrameDeliverer::
    AddSubCaptureTargetVersionCallbackOnVideoTaskRunner(
        uint32_t sub_capture_target_version,
        WTF::CrossThreadOnceClosure callback) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(sub_capture_target_version_callbacks_,
                         sub_capture_target_version));

  sub_capture_target_version_callbacks_.Set(sub_capture_target_version,
                                            std::move(callback));
}

void MediaStreamVideoTrack::FrameDeliverer::
    RemoveSubCaptureTargetVersionCallbackOnVideoTaskRunner(
        uint32_t sub_capture_target_version) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());

  // Note: Might or might not be here, depending on whether a later crop
  // version has already been observed or not.
  sub_capture_target_version_callbacks_.erase(sub_capture_target_version);
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnVideoTaskRunner(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());

  frame_drop_log_state_ = FrameDropLogState();

  // TODO(crbug.com/1369085): Understand why we sometimes see old
  // sub-capture-target versions.
  if (frame->metadata().sub_capture_target_version !=
      sub_capture_target_version_) {
    OnFrameDroppedOnVideoTaskRunner(
        media::VideoCaptureFrameDropReason::kSubCaptureTargetVersionNotCurrent);
    return;
  }

  if (!enabled_ && emit_frame_drop_events_) {
    emit_frame_drop_events_ = false;
    LogFrameDroppedOnVideoTaskRunner(
        media::VideoCaptureFrameDropReason::
            kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame);
  }
  scoped_refptr<media::VideoFrame> video_frame;
  if (enabled_) {
    video_frame = std::move(frame);
    ++deliverable_frames_;
  } else {
    // When disabled, a black video frame is passed along instead. The original
    // frames are dropped.
    video_frame = GetBlackFrame(*frame);
  }
  for (const auto& entry : callbacks_) {
    entry.deliver_frame.Run(video_frame, estimated_capture_time);
  }
  // The delay on refresh timer is reset each time a frame is received so that
  // it will not fire for at least an additional period. This means refresh
  // frames will only be requested when the source has halted delivery (e.g., a
  // screen capturer stops sending frames because the screen is not being
  // updated).
  if (is_refreshing_for_min_frame_rate_) {
    PostCrossThreadTask(
        *main_render_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&MediaStreamVideoTrack::ResetRefreshTimer,
                            media_stream_video_track_));
  }
}

void MediaStreamVideoTrack::FrameDeliverer::OnFrameDroppedOnVideoTaskRunner(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << __func__;
  LogFrameDroppedOnVideoTaskRunner(reason);
  if (enabled_) {
    if (reason == media::VideoCaptureFrameDropReason::
                      kResolutionAdapterFrameRateIsHigherThanRequested) {
      ++discarded_frames_;
    } else {
      ++dropped_frames_;
    }
  }
  // Notify sinks that care about frame drops, i.e. WebRTC.
  for (const auto& entry : callbacks_) {
    entry.notify_frame_dropped.Run(reason);
  }
}

void MediaStreamVideoTrack::FrameDeliverer::LogFrameDroppedOnVideoTaskRunner(
    media::VideoCaptureFrameDropReason reason) {
  MaybeEmitFrameDropLogMessage(reason);

  if (reason == frame_drop_log_state_.drop_reason) {
    if (frame_drop_log_state_.max_log_count_exceeded) {
      return;
    }

    if (++frame_drop_log_state_.drop_count >
        kMaxConsecutiveFrameDropForSameReasonCount) {
      frame_drop_log_state_.max_log_count_exceeded = true;
      return;
    }
  } else {
    frame_drop_log_state_ = FrameDropLogState(reason);
  }

  LogVideoFrameDropUMA(reason, stream_type_);
}

void MediaStreamVideoTrack::FrameDeliverer::MaybeEmitFrameDropLogMessage(
    media::VideoCaptureFrameDropReason reason) {
  using Type = std::underlying_type<media::VideoCaptureFrameDropReason>::type;
  static_assert(
      static_cast<Type>(media::VideoCaptureFrameDropReason::kMaxValue) <= 100,
      "Risk of memory overuse.");

  static_assert(kMaxEmittedLogsForDroppedFramesBeforeSuppressing <
                    kFrequencyForSuppressedLogs,
                "");

  DCHECK_GE(static_cast<Type>(reason), 0);
  DCHECK_LE(reason, media::VideoCaptureFrameDropReason::kMaxValue);

  int& occurrences = frame_drop_log_counters_[reason];
  if (++occurrences > kMaxEmittedLogsForDroppedFramesBeforeSuppressing &&
      occurrences % kFrequencyForSuppressedLogs != 0) {
    return;
  }

  std::ostringstream string_stream;
  string_stream << "Frame dropped with reason code "
                << static_cast<Type>(reason) << ".";
  if (occurrences == kMaxEmittedLogsForDroppedFramesBeforeSuppressing) {
    string_stream << " Additional logs will be partially suppressed.";
  }

  // EmitLogMessage() unless overridden by testing.
  emit_log_message_.Run(string_stream.str());
}

void MediaStreamVideoTrack::FrameDeliverer::SetEmitLogMessage(
    base::RepeatingCallback<void(const std::string&)> emit_log_message) {
  emit_log_message_ = std::move(emit_log_message);
}

void MediaStreamVideoTrack::FrameDeliverer::EmitLogMessage(
    const std::string& message) {
  PostCrossThreadTask(*main_render_task_runner_, FROM_HERE,
                      CrossThreadBindOnce(&MediaStreamVideoSource::OnLog,
                                          media_stream_video_source_, message));
}

void MediaStreamVideoTrack::FrameDeliverer::
    DeliverEncodedVideoFrameOnVideoTaskRunner(
        scoped_refptr<EncodedVideoFrame> frame,
        base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  if (!enabled_) {
    return;
  }
  if (await_next_key_frame_ && !frame->IsKeyFrame()) {
    return;
  }
  await_next_key_frame_ = false;
  for (const auto& entry : encoded_callbacks_.Values()) {
    entry.Run(frame, estimated_capture_time);
  }
}

void MediaStreamVideoTrack::FrameDeliverer::
    NewSubCaptureTargetVersionOnVideoTaskRunner(
        uint32_t sub_capture_target_version) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(sub_capture_target_version, sub_capture_target_version_);

  sub_capture_target_version_ = sub_capture_target_version;

  Vector<uint32_t> to_be_removed_keys;
  for (auto& iter : sub_capture_target_version_callbacks_) {
    if (iter.key > sub_capture_target_version) {
      continue;
    }
    std::move(iter.value).Run();
    to_be_removed_keys.push_back(iter.key);
  }
  sub_capture_target_version_callbacks_.RemoveAll(to_be_removed_keys);
}

scoped_refptr<media::VideoFrame>
MediaStreamVideoTrack::FrameDeliverer::GetBlackFrame(
    const media::VideoFrame& reference_frame) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  if (!black_frame_.get() ||
      black_frame_->natural_size() != reference_frame.natural_size()) {
    black_frame_ =
        media::VideoFrame::CreateBlackFrame(reference_frame.natural_size());
  }

  // Wrap |black_frame_| so we get a fresh timestamp we can modify. Frames
  // returned from this function may still be in use.
  scoped_refptr<media::VideoFrame> wrapped_black_frame =
      media::VideoFrame::WrapVideoFrame(black_frame_, black_frame_->format(),
                                        black_frame_->visible_rect(),
                                        black_frame_->natural_size());
  if (!wrapped_black_frame)
    return nullptr;

  wrapped_black_frame->set_timestamp(reference_frame.timestamp());
  wrapped_black_frame->metadata().reference_time =
      reference_frame.metadata().reference_time;

  return wrapped_black_frame;
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled) {
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source->Owner(), std::make_unique<MediaStreamVideoTrack>(
                           source, std::move(callback), enabled));
  return WebMediaStreamTrack(component);
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const std::optional<bool>& noise_reduction,
    bool is_screencast,
    const std::optional<double>& min_frame_rate,
    const ImageCaptureDeviceSettings* image_capture_device_settings,
    bool pan_tilt_zoom_allowed,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled) {
  WebMediaStreamTrack track;
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source->Owner(),
      std::make_unique<MediaStreamVideoTrack>(
          source, adapter_settings, noise_reduction, is_screencast,
          min_frame_rate, image_capture_device_settings, pan_tilt_zoom_allowed,
          std::move(callback), enabled));
  return WebMediaStreamTrack(component);
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::From(
    const MediaStreamComponent* component) {
  if (!component ||
      component->GetSourceType() != MediaStreamSource::kTypeVideo) {
    return nullptr;
  }

  return static_cast<MediaStreamVideoTrack*>(component->GetPlatformTrack());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled)
    : MediaStreamTrackPlatform(true),
      is_screencast_(false),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->GetTaskRunner(), source->video_task_runner(),
          weak_factory_.GetWeakPtr(), source->GetWeakPtr(), enabled,
          source->GetSubCaptureTargetVersion());
  source->AddTrack(
      this, VideoTrackAdapterSettings(),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnVideoTaskRunner,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&MediaStreamVideoTrack::FrameDeliverer::
                                       OnFrameDroppedOnVideoTaskRunner,
                                   frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::
              DeliverEncodedVideoFrameOnVideoTaskRunner,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::
              NewSubCaptureTargetVersionOnVideoTaskRunner,
          frame_deliverer_)),
      base::BindPostTaskToCurrentDefault(WTF::BindRepeating(
          &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
          weak_factory_.GetWeakPtr())),
      base::BindPostTaskToCurrentDefault(
          WTF::BindRepeating(&MediaStreamVideoTrack::set_computed_source_format,
                             weak_factory_.GetWeakPtr())),
      std::move(callback));
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const std::optional<bool>& noise_reduction,
    bool is_screen_cast,
    const std::optional<double>& min_frame_rate,
    const ImageCaptureDeviceSettings* image_capture_device_settings,
    bool pan_tilt_zoom_allowed,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled)
    : MediaStreamTrackPlatform(true),
      adapter_settings_(adapter_settings),
      noise_reduction_(noise_reduction),
      is_screencast_(is_screen_cast),
      min_frame_rate_(min_frame_rate),
      image_capture_device_settings_(
          image_capture_device_settings
              ? std::make_optional(*image_capture_device_settings)
              : std::nullopt),
      pan_tilt_zoom_allowed_(pan_tilt_zoom_allowed),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->GetTaskRunner(), source->video_task_runner(),
          weak_factory_.GetWeakPtr(), source->GetWeakPtr(), enabled,
          source->GetSubCaptureTargetVersion());
  source->AddTrack(
      this, adapter_settings,
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnVideoTaskRunner,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&MediaStreamVideoTrack::FrameDeliverer::
                                       OnFrameDroppedOnVideoTaskRunner,
                                   frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::
              DeliverEncodedVideoFrameOnVideoTaskRunner,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::
              NewSubCaptureTargetVersionOnVideoTaskRunner,
          frame_deliverer_)),
      base::BindPostTaskToCurrentDefault(WTF::BindRepeating(
          &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
          weak_factory_.GetWeakPtr())),
      base::BindPostTaskToCurrentDefault(
          WTF::BindRepeating(&MediaStreamVideoTrack::set_computed_source_format,
                             weak_factory_.GetWeakPtr())),
      std::move(callback));
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(sinks_.empty());
  DCHECK(encoded_sinks_.empty());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

std::unique_ptr<MediaStreamTrackPlatform>
MediaStreamVideoTrack::CreateFromComponent(
    const MediaStreamComponent* component,
    const String& id) {
  MediaStreamSource* source = component->Source();
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  MediaStreamVideoTrack* original_track =
      MediaStreamVideoTrack::From(component);
  DCHECK(original_track);
  return std::make_unique<MediaStreamVideoTrack>(
      native_source, original_track->adapter_settings(),
      original_track->noise_reduction(), original_track->is_screencast(),
      original_track->min_frame_rate(),
      original_track->image_capture_device_settings()
          ? &*original_track->image_capture_device_settings()
          : nullptr,
      original_track->pan_tilt_zoom_allowed(),
      MediaStreamVideoSource::ConstraintsOnceCallback(), component->Enabled());
}

static void AddSinkInternal(Vector<WebMediaStreamSink*>* sinks,
                            WebMediaStreamSink* sink) {
  DCHECK(!base::Contains(*sinks, sink));
  sinks->push_back(sink);
}

static void RemoveSinkInternal(Vector<WebMediaStreamSink*>* sinks,
                               WebMediaStreamSink* sink) {
  auto it = base::ranges::find(*sinks, sink);
  CHECK(it != sinks->end(), base::NotFatalUntil::M130);
  sinks->erase(it);
}

void MediaStreamVideoTrack::AddSink(
    WebMediaStreamSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    MediaStreamVideoSink::IsSecure is_secure,
    MediaStreamVideoSink::UsesAlpha uses_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  AddSinkInternal(&sinks_, sink);
  frame_deliverer_->AddCallback(sink, callback);
  secure_tracker_.Add(sink, is_secure == MediaStreamVideoSink::IsSecure::kYes);
  if (uses_alpha == MediaStreamVideoSink::UsesAlpha::kDefault) {
    alpha_using_sinks_.insert(sink);
  } else if (uses_alpha == MediaStreamVideoSink::UsesAlpha::kNo) {
    alpha_discarding_sinks_.insert(sink);
  }

  // Ensure sink gets told about any constraints set.
  sink->OnVideoConstraintsChanged(min_frame_rate_,
                                  adapter_settings_.max_frame_rate());

  // Request source to deliver a frame because a new sink is added.
  if (!source_)
    return;
  UpdateSourceHasConsumers();
  RequestRefreshFrame();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());

  source_->UpdateCanDiscardAlpha();

  if (is_screencast_)
    StartTimerForRequestingFrames();
}

bool MediaStreamVideoTrack::UsingAlpha() const {
  // Alpha can't be discarded if any sink uses alpha, or if the only sinks
  // connected are kDependsOnOtherSinks.
  bool only_sinks_with_alpha_depending_on_other_sinks =
      !sinks_.empty() && alpha_using_sinks_.empty() &&
      alpha_discarding_sinks_.empty();
  return !alpha_using_sinks_.empty() ||
         only_sinks_with_alpha_depending_on_other_sinks;
}

gfx::Size MediaStreamVideoTrack::GetVideoSize() const {
  return gfx::Size(width_, height_);
}

void MediaStreamVideoTrack::SetSinkNotifyFrameDroppedCallback(
    WebMediaStreamSink* sink,
    const VideoCaptureNotifyFrameDroppedCB& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DVLOG(1) << __func__;
  frame_deliverer_->SetNotifyFrameDroppedCallback(sink, callback);
}

void MediaStreamVideoTrack::AddEncodedSink(WebMediaStreamSink* sink,
                                           EncodedVideoFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  AddSinkInternal(&encoded_sinks_, sink);
  frame_deliverer_->AddEncodedCallback(sink, std::move(callback));
  if (source_)
    source_->UpdateNumEncodedSinks();
  UpdateSourceHasConsumers();
}

void MediaStreamVideoTrack::RemoveSink(WebMediaStreamSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  RemoveSinkInternal(&sinks_, sink);
  alpha_using_sinks_.erase(sink);
  alpha_discarding_sinks_.erase(sink);
  frame_deliverer_->RemoveCallback(sink);
  secure_tracker_.Remove(sink);
  if (!source_)
    return;
  UpdateSourceHasConsumers();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());

  source_->UpdateCanDiscardAlpha();
  // Restart the timer with existing sinks.
  if (is_screencast_)
    StartTimerForRequestingFrames();
}

void MediaStreamVideoTrack::RemoveEncodedSink(WebMediaStreamSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  RemoveSinkInternal(&encoded_sinks_, sink);
  frame_deliverer_->RemoveEncodedCallback(sink);
  if (source_)
    source_->UpdateNumEncodedSinks();
  UpdateSourceHasConsumers();
}

void MediaStreamVideoTrack::UpdateSourceHasConsumers() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;
  bool has_consumers = !sinks_.empty() || !encoded_sinks_.empty();
  source_->UpdateHasConsumers(this, has_consumers);
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  // If enabled, encoded sinks exist and the source supports encoded output, we
  // need a new keyframe from the source as we may have dropped data making the
  // stream undecodable.
  bool maybe_await_key_frame = false;
  if (enabled && source_ && source_->SupportsEncodedOutput() &&
      !encoded_sinks_.empty()) {
    RequestRefreshFrame();
    maybe_await_key_frame = true;
  }
  frame_deliverer_->SetEnabled(enabled, maybe_await_key_frame);
  for (auto* sink : sinks_)
    sink->OnEnabledChanged(enabled);
  for (auto* encoded_sink : encoded_sinks_)
    encoded_sink->OnEnabledChanged(enabled);
}

size_t MediaStreamVideoTrack::CountSinks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return sinks_.size();
}

size_t MediaStreamVideoTrack::CountEncodedSinks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return encoded_sinks_.size();
}

void MediaStreamVideoTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_)
    sink->OnContentHintChanged(content_hint);
  for (auto* encoded_sink : encoded_sinks_)
    encoded_sink->OnContentHintChanged(content_hint);
}

void MediaStreamVideoTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (source_) {
    source_->RemoveTrack(this, std::move(callback));
    source_ = nullptr;
  } else if (callback) {
    std::move(callback).Run();
  }
  OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
  refresh_timer_.Stop();
}

void MediaStreamVideoTrack::GetSettings(
    MediaStreamTrackPlatform::Settings& settings) const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;

  if (width_ && height_) {
    settings.width = width_;
    settings.height = height_;
    settings.aspect_ratio = static_cast<double>(width_) / height_;
  }

  if (std::optional<media::VideoCaptureFormat> format =
          source_->GetCurrentFormat()) {
    // For local capture-based tracks, the frame rate returned by
    // MediaStreamTrack.getSettings() must be the configured frame rate. In case
    // of frame rate decimation, the configured frame rate is the decimated
    // frame rate (i.e., the adapter frame rate). If there is no decimation, the
    // configured frame rate is the frame rate reported by the device.
    // Decimation occurs only when the adapter frame rate is lower than the
    // device frame rate.
    std::optional<double> adapter_frame_rate =
        adapter_settings_.max_frame_rate();
    settings.frame_rate =
        (!adapter_frame_rate || *adapter_frame_rate > format->frame_rate)
            ? format->frame_rate
            : *adapter_frame_rate;
  } else {
    // For other tracks, use the computed frame rate reported via
    // SetSizeAndComputedFrameRate().
    if (computed_frame_rate_)
      settings.frame_rate = *computed_frame_rate_;
  }

  settings.facing_mode = ToPlatformFacingMode(
      static_cast<mojom::blink::FacingMode>(source_->device().video_facing));
  settings.resize_mode = WebString::FromASCII(std::string(
      adapter_settings().target_size() ? WebMediaStreamTrack::kResizeModeRescale
                                       : WebMediaStreamTrack::kResizeModeNone));
  if (source_->device().display_media_info) {
    const auto& info = source_->device().display_media_info;
    settings.display_surface = info->display_surface;
    settings.logical_surface = info->logical_surface;
    settings.cursor = info->cursor;
  }
}

MediaStreamTrackPlatform::VideoFrameStats
MediaStreamVideoTrack::GetVideoFrameStats() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  MediaStreamTrackPlatform::VideoFrameStats stats;
  stats.deliverable_frames = frame_deliverer_->deliverable_frames();
  stats.discarded_frames = frame_deliverer_->discarded_frames();
  stats.dropped_frames = frame_deliverer_->dropped_frames();
  return stats;
}

MediaStreamTrackPlatform::CaptureHandle
MediaStreamVideoTrack::GetCaptureHandle() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  MediaStreamTrackPlatform::CaptureHandle capture_handle;

  if (!source_) {
    return capture_handle;
  }

  const MediaStreamDevice& device = source_->device();
  if (!device.display_media_info) {
    return capture_handle;
  }
  const media::mojom::DisplayMediaInformationPtr& info =
      device.display_media_info;

  if (!info->capture_handle) {
    return capture_handle;
  }

  if (!info->capture_handle->origin.opaque()) {
    capture_handle.origin =
        String::FromUTF8(info->capture_handle->origin.Serialize());
  }
  capture_handle.handle =
      WebString::FromUTF16(info->capture_handle->capture_handle);

  return capture_handle;
}

void MediaStreamVideoTrack::AddSubCaptureTargetVersionCallback(
    uint32_t sub_capture_target_version,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  frame_deliverer_->AddSubCaptureTargetVersionCallback(
      sub_capture_target_version,
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback)));
}

void MediaStreamVideoTrack::RemoveSubCaptureTargetVersionCallback(
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  frame_deliverer_->RemoveSubCaptureTargetVersionCallback(
      sub_capture_target_version);
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // Copy the vectors first, since sinks might DisconnectFromTrack() and
  // invalidate iterators.

  Vector<WebMediaStreamSink*> sinks_copy(sinks_);
  for (auto* sink : sinks_copy)
    sink->OnReadyStateChanged(state);

  Vector<WebMediaStreamSink*> encoded_sinks_copy(encoded_sinks_);
  for (auto* encoded_sink : encoded_sinks_copy)
    encoded_sink->OnReadyStateChanged(state);
}

void MediaStreamVideoTrack::SetMinimumFrameRate(double min_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  min_frame_rate_ = min_frame_rate;
}

void MediaStreamVideoTrack::SetTrackAdapterSettings(
    const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  adapter_settings_ = settings;
}

void MediaStreamVideoTrack::NotifyConstraintsConfigurationComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_) {
    sink->OnVideoConstraintsChanged(min_frame_rate_,
                                    adapter_settings_.max_frame_rate());
  }

  if (is_screencast_) {
    StartTimerForRequestingFrames();
  }
}

media::VideoCaptureFormat MediaStreamVideoTrack::GetComputedSourceFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return computed_source_format_;
}

void MediaStreamVideoTrack::OnSinkDroppedFrame(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_) {
    return;
  }
  PostCrossThreadTask(
      *source_->video_task_runner(), FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::LogFrameDroppedOnVideoTaskRunner,
                          frame_deliverer_, reason));
}

void MediaStreamVideoTrack::SetEmitLogMessageForTesting(
    base::RepeatingCallback<void(const std::string&)> emit_log_message) {
  frame_deliverer_->SetEmitLogMessage(std::move(emit_log_message));
}

void MediaStreamVideoTrack::StartTimerForRequestingFrames() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // Find the maximum of all the required min frames per second in the attached
  // sinks.
  double required_min_fps = 0;
  for (auto* web_sink : sinks_) {
    auto* sink = static_cast<MediaStreamVideoSink*>(web_sink);
    required_min_fps =
        std::max(required_min_fps, sink->GetRequiredMinFramesPerSec());
  }

  base::TimeDelta refresh_interval = ComputeRefreshIntervalFromBounds(
      base::Hertz(required_min_fps), min_frame_rate(), max_frame_rate());

  if (refresh_interval.is_max()) {
    refresh_timer_.Stop();
    frame_deliverer_->SetIsRefreshingForMinFrameRate(false);
  } else {
    DVLOG(1) << "Starting frame refresh timer with interval "
             << refresh_interval.InMillisecondsF() << " ms.";
    refresh_timer_.Start(FROM_HERE, refresh_interval, this,
                         &MediaStreamVideoTrack::RequestRefreshFrame);
    frame_deliverer_->SetIsRefreshingForMinFrameRate(true);
  }
}

void MediaStreamVideoTrack::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (source_)
    source_->RequestRefreshFrame();
}

void MediaStreamVideoTrack::ResetRefreshTimer() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (refresh_timer_.IsRunning())
    refresh_timer_.Reset();
}

}  // namespace blink
