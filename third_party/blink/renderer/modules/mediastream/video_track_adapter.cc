// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace WTF {

// Template specializations of [1], needed to be able to pass WTF callbacks
// that have VideoTrackAdapterSettings or gfx::Size parameters across threads.
//
// [1] third_party/blink/renderer/platform/wtf/cross_thread_copier.h.
template <>
struct CrossThreadCopier<blink::VideoTrackAdapterSettings>
    : public CrossThreadCopierPassThrough<blink::VideoTrackAdapterSettings> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

// Amount of frame intervals to wait before considering the source as muted, for
// the first frame and under normal conditions, respectively. First frame might
// take longer to arrive due to source startup.
const float kFirstFrameTimeoutInFrameIntervals = 100.0f;
const float kNormalFrameTimeoutInFrameIntervals = 25.0f;

// |kMaxDeltaDeviationFactor| is used to determine |max_delta_deviation_| which
// specifies the allowed deviation from |target_delta_| before dropping a frame.
// It's set to 20% to be aligned with the previous logic in this file.
constexpr float kMaxDeltaDeviationFactor = 0.2;

// If the delta between two frames is bigger than this, we will consider it to
// be invalid and reset the fps calculation.
constexpr base::TimeDelta kMaxTimeBetweenFrames = base::Milliseconds(1000);

constexpr base::TimeDelta kFrameRateChangeInterval = base::Seconds(1);
const double kFrameRateChangeRate = 0.01;
constexpr base::TimeDelta kFrameRateUpdateInterval = base::Seconds(5);

struct ComputedSettings {
  gfx::Size frame_size;
  double frame_rate = MediaStreamVideoSource::kDefaultFrameRate;
  double last_updated_frame_rate = MediaStreamVideoSource::kDefaultFrameRate;
  base::TimeDelta prev_frame_timestamp = base::TimeDelta::Max();
  base::TimeTicks new_frame_rate_timestamp;
  base::TimeTicks last_update_timestamp;
};

int ClampToValidDimension(int dimension) {
  return std::min(static_cast<int>(media::limits::kMaxDimension),
                  std::max(0, dimension));
}

void ComputeFrameRate(const base::TimeDelta& frame_timestamp,
                      double* frame_rate,
                      base::TimeDelta* prev_frame_timestamp) {
  const double delta_ms =
      (frame_timestamp - *prev_frame_timestamp).InMillisecondsF();
  *prev_frame_timestamp = frame_timestamp;
  if (delta_ms < 0)
    return;

  *frame_rate = 200 / delta_ms + 0.8 * *frame_rate;
}

// Controls the frequency of settings updates based on frame rate changes.
// Returns |true| if over the last second the computed frame rate is
// consistently kFrameRateChangeRate different than the last reported value,
// or if there hasn't been any update in the last
// kFrameRateUpdateIntervalInSeconds seconds.
bool MaybeUpdateFrameRate(ComputedSettings* settings) {
  base::TimeTicks now = base::TimeTicks::Now();

  // Update frame rate if over the last second the computed frame rate has been
  // consistently kFrameRateChangeIntervalInSeconds different than the last
  // reported value.
  if (std::abs(settings->frame_rate - settings->last_updated_frame_rate) >
      settings->last_updated_frame_rate * kFrameRateChangeRate) {
    if (now - settings->new_frame_rate_timestamp > kFrameRateChangeInterval) {
      settings->new_frame_rate_timestamp = now;
      settings->last_update_timestamp = now;
      settings->last_updated_frame_rate = settings->frame_rate;
      return true;
    }
  } else {
    settings->new_frame_rate_timestamp = now;
  }

  // Update frame rate if it hasn't been updated in the last
  // kFrameRateUpdateIntervalInSeconds seconds.
  if (now - settings->last_update_timestamp > kFrameRateUpdateInterval) {
    settings->last_update_timestamp = now;
    settings->last_updated_frame_rate = settings->frame_rate;
    return true;
  }
  return false;
}

VideoTrackAdapterSettings ReturnSettingsMaybeOverrideMaxFps(
    const VideoTrackAdapterSettings& settings) {
  VideoTrackAdapterSettings new_settings = settings;
  std::optional<double> max_fps_override =
      Platform::Current()->GetWebRtcMaxCaptureFrameRate();
  if (max_fps_override) {
    DVLOG(1) << "Overriding max frame rate.  Was="
             << settings.max_frame_rate().value_or(-1)
             << ", Now=" << *max_fps_override;
    new_settings.set_max_frame_rate(*max_fps_override);
  }
  return new_settings;
}

}  // anonymous namespace

// VideoFrameResolutionAdapter is created on and lives on the video task runner.
// It does the resolution adaptation and delivers frames to all registered
// tracks on the video task runner. All method calls must be on the video task
// runner.
class VideoTrackAdapter::VideoFrameResolutionAdapter
    : public WTF::ThreadSafeRefCounted<VideoFrameResolutionAdapter> {
 public:
  struct VideoTrackCallbacks {
    VideoCaptureDeliverFrameInternalCallback frame_callback;
    VideoCaptureNotifyFrameDroppedInternalCallback
        notify_frame_dropped_callback;
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback;
    VideoCaptureSubCaptureTargetVersionInternalCallback
        sub_capture_target_version_callback;
    VideoTrackSettingsInternalCallback settings_callback;
    VideoTrackFormatInternalCallback format_callback;
  };
  // Setting |max_frame_rate| to 0.0, means that no frame rate limitation
  // will be done.
  VideoFrameResolutionAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> reader_task_runner,
      const VideoTrackAdapterSettings& settings,
      base::WeakPtr<MediaStreamVideoSource> media_stream_video_source);

  VideoFrameResolutionAdapter(const VideoFrameResolutionAdapter&) = delete;
  VideoFrameResolutionAdapter& operator=(const VideoFrameResolutionAdapter&) =
      delete;

  // Add |frame_callback|, |encoded_frame_callback| to receive video frames on
  // the video task runner, |sub_capture_target_version_callback| to receive
  // notifications when a new sub-capture-target version is acknowledged, and
  // |settings_callback| to set track settings on the main thread.
  // |frame_callback| will however be released on the main render thread.
  void AddCallbacks(
      const MediaStreamVideoTrack* track,
      VideoCaptureDeliverFrameInternalCallback frame_callback,
      VideoCaptureNotifyFrameDroppedInternalCallback
          notify_frame_dropped_callback,
      DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
      VideoCaptureSubCaptureTargetVersionInternalCallback
          sub_capture_target_version_callback,
      VideoTrackSettingsInternalCallback settings_callback,
      VideoTrackFormatInternalCallback format_callback);

  // Removes the callbacks associated with |track| if |track| has been added. It
  // is ok to call RemoveCallbacks() even if |track| has not been added.
  void RemoveCallbacks(const MediaStreamVideoTrack* track);

  // Removes the callbacks associated with |track| if |track| has been added. It
  // is ok to call RemoveAndGetCallbacks() even if the |track| has not been
  // added. The function returns the callbacks if it was removed, or empty
  // callbacks if |track| was not present in the adapter.
  VideoTrackCallbacks RemoveAndGetCallbacks(const MediaStreamVideoTrack* track);

  // The source has provided us with a frame.
  void DeliverFrame(
      scoped_refptr<media::VideoFrame> frame,
      const base::TimeTicks& estimated_capture_time,
      bool is_device_rotated);
  // This method is called when a frame is dropped, whether dropped by the
  // source (via VideoTrackAdapter::OnFrameDroppedOnVideoTaskRunner) or
  // internally (in DeliverFrame).
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason);

  void DeliverEncodedVideoFrame(scoped_refptr<EncodedVideoFrame> frame,
                                base::TimeTicks estimated_capture_time);

  void NewSubCaptureTargetVersionOnVideoTaskRunner(
      uint32_t sub_capture_target_version);

  // Returns true if all arguments match with the output of this adapter.
  bool SettingsMatch(const VideoTrackAdapterSettings& settings) const;

  bool IsEmpty() const;

  // Sets frame rate to 0.0 if frame monitor has detected muted state.
  void ResetFrameRate();

 private:
  virtual ~VideoFrameResolutionAdapter();
  friend class WTF::ThreadSafeRefCounted<VideoFrameResolutionAdapter>;

  void DoDeliverFrame(
      scoped_refptr<media::VideoFrame> video_frame,
      const base::TimeTicks& estimated_capture_time);

  // Returns |true| if the input frame rate is higher that the requested max
  // frame rate and |frame| should be dropped. If it returns true, |reason| is
  // assigned to indicate the particular reason for the decision.
  bool MaybeDropFrame(const media::VideoFrame& frame,
                      float source_frame_rate,
                      media::VideoCaptureFrameDropReason* reason);

  // Updates track settings if either frame width, height or frame rate have
  // changed since last update.
  void MaybeUpdateTrackSettings(
      const VideoTrackSettingsInternalCallback& settings_callback,
      const media::VideoFrame& frame);

  // Updates computed source format for all tracks if either frame width, height
  // or frame rate have changed since last update.
  void MaybeUpdateTracksFormat(const media::VideoFrame& frame);

  // Bound to the video task runner.
  SEQUENCE_CHECKER(video_sequence_checker_);

  // The task runner where we will release VideoCaptureDeliverFrameCB
  // registered in AddCallbacks.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  base::WeakPtr<MediaStreamVideoSource> media_stream_video_source_;

  const VideoTrackAdapterSettings settings_;

  // The target timestamp delta between video frames, corresponding to the max
  // fps.
  const std::optional<base::TimeDelta> target_delta_;

  // The maximum allowed deviation from |target_delta_| before dropping a frame.
  const std::optional<base::TimeDelta> max_delta_deviation_;

  // The timestamp of the last delivered video frame.
  base::TimeDelta timestamp_last_delivered_frame_ = base::TimeDelta::Max();

  // Stores the accumulated difference between |target_delta_| and the actual
  // timestamp delta between frames that are delivered. Clamped to
  // [-max_delta_deviation, target_delta_ / 2]. This is used to allow some
  // frames to be closer than |target_delta_| in order to maintain
  // |target_delta_| on average. Without it we may end up with an average fps
  // that is half of max fps.
  base::TimeDelta accumulated_drift_;

  ComputedSettings track_settings_;
  ComputedSettings source_format_settings_;

  base::flat_map<const MediaStreamVideoTrack*, VideoTrackCallbacks> callbacks_;
};

VideoTrackAdapter::VideoFrameResolutionAdapter::VideoFrameResolutionAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> reader_task_runner,
    const VideoTrackAdapterSettings& settings,
    base::WeakPtr<MediaStreamVideoSource> media_stream_video_source)
    : renderer_task_runner_(reader_task_runner),
      media_stream_video_source_(media_stream_video_source),
      settings_(ReturnSettingsMaybeOverrideMaxFps(settings)),
      target_delta_(settings_.max_frame_rate()
                        ? std::make_optional(base::Seconds(
                              1.0 / settings_.max_frame_rate().value()))
                        : std::nullopt),
      max_delta_deviation_(target_delta_
                               ? std::make_optional(kMaxDeltaDeviationFactor *
                                                    target_delta_.value())
                               : std::nullopt) {
  DVLOG(1) << __func__ << " max_framerate "
           << settings.max_frame_rate().value_or(-1);
  DCHECK(renderer_task_runner_.get());
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  CHECK_NE(0, settings_.max_aspect_ratio());
}

VideoTrackAdapter::VideoFrameResolutionAdapter::~VideoFrameResolutionAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  DCHECK(callbacks_.empty());
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::AddCallbacks(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameInternalCallback frame_callback,
    VideoCaptureNotifyFrameDroppedInternalCallback
        notify_frame_dropped_callback,
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionInternalCallback
        sub_capture_target_version_callback,
    VideoTrackSettingsInternalCallback settings_callback,
    VideoTrackFormatInternalCallback format_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);

  // The new track's settings should match the resolution adapter's current
  // |track_settings_| as set for existing track(s) with matching
  // VideoTrackAdapterSettings.
  if (!callbacks_.empty() && track_settings_.frame_size.width() > 0 &&
      track_settings_.frame_size.height() > 0) {
    settings_callback.Run(track_settings_.frame_size,
                          track_settings_.frame_rate);
  }

  VideoTrackCallbacks track_callbacks = {
      std::move(frame_callback),
      std::move(notify_frame_dropped_callback),
      std::move(encoded_frame_callback),
      std::move(sub_capture_target_version_callback),
      std::move(settings_callback),
      std::move(format_callback)};
  callbacks_.emplace(track, std::move(track_callbacks));
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveCallbacks(
    const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  callbacks_.erase(track);
}

VideoTrackAdapter::VideoFrameResolutionAdapter::VideoTrackCallbacks
VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveAndGetCallbacks(
    const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  VideoTrackCallbacks track_callbacks;
  auto it = callbacks_.find(track);
  if (it == callbacks_.end())
    return track_callbacks;

  track_callbacks = std::move(it->second);
  callbacks_.erase(it);
  return track_callbacks;
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& estimated_capture_time,
    bool is_device_rotated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);

  if (!video_frame) {
    DLOG(ERROR) << "Incoming frame is not valid.";
    OnFrameDropped(
        media::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid);
    return;
  }

  ComputeFrameRate(video_frame->timestamp(),
                   &source_format_settings_.frame_rate,
                   &source_format_settings_.prev_frame_timestamp);
  MaybeUpdateTracksFormat(*video_frame);

  double frame_rate = video_frame->metadata().frame_rate.value_or(
      MediaStreamVideoSource::kUnknownFrameRate);

  auto frame_drop_reason = media::VideoCaptureFrameDropReason::kNone;
  if (MaybeDropFrame(*video_frame, frame_rate, &frame_drop_reason)) {
    OnFrameDropped(frame_drop_reason);
    return;
  }

  // If the frame is a texture not backed up by GPU memory we don't apply
  // cropping/scaling and deliver the frame as-is, leaving it up to the
  // destination to rescale it. Otherwise, cropping and scaling is soft-applied
  // before delivery for efficiency.
  //
  // TODO(crbug.com/362521): Allow cropping/scaling of non-GPU memory backed
  // textures.
  if (video_frame->HasSharedImage() &&
      video_frame->storage_type() !=
          media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DoDeliverFrame(std::move(video_frame), estimated_capture_time);
    return;
  }
  // The video frame we deliver may or may not get cropping and scaling
  // soft-applied. Ultimately the listener will decide whether to use the
  // |delivered_video_frame|.
  scoped_refptr<media::VideoFrame> delivered_video_frame = video_frame;

  gfx::Size desired_size;
  CalculateDesiredSize(is_device_rotated, video_frame->natural_size(),
                       settings_, &desired_size);
  if (desired_size != video_frame->natural_size()) {
    // Get the largest centered rectangle with the same aspect ratio of
    // |desired_size| that fits entirely inside of
    // |video_frame->visible_rect()|. This will be the rect we need to crop the
    // original frame to. From this rect, the original frame can be scaled down
    // to |desired_size|.
    gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
        video_frame->visible_rect(), desired_size);

    // Some consumers (for example
    // ImageCaptureFrameGrabber::SingleShotFrameHandler::ConvertAndDeliverFrame)
    // don't support pixel format conversions when the source format is YUV with
    // UV subsampled and vsible_rect().x() being odd. The conversion ends up
    // miscomputing the UV plane and ends up with a VU plane leading to a blue
    // face tint. Round x() to even to avoid. See crbug.com/1307304.
    region_in_frame.set_x(region_in_frame.x() & ~1);
    region_in_frame.set_y(region_in_frame.y() & ~1);

    // ComputeLetterboxRegion() sometimes produces odd dimensions due to
    // internal rounding errors; allow to round upwards if there's slack
    // otherwise round downwards.
    bool width_has_slack =
        region_in_frame.right() < video_frame->visible_rect().right();
    region_in_frame.set_width((region_in_frame.width() + width_has_slack) & ~1);
    bool height_has_slack =
        region_in_frame.bottom() < video_frame->visible_rect().bottom();
    region_in_frame.set_height((region_in_frame.height() + height_has_slack) &
                               ~1);

    delivered_video_frame = media::VideoFrame::WrapVideoFrame(
        video_frame, video_frame->format(), region_in_frame, desired_size);
    if (!delivered_video_frame) {
      OnFrameDropped(media::VideoCaptureFrameDropReason::
                         kResolutionAdapterWrappingFrameForCroppingFailed);
      return;
    }

    DVLOG(3) << "desired size  " << desired_size.ToString()
             << " output natural size "
             << delivered_video_frame->natural_size().ToString()
             << " output visible rect  "
             << delivered_video_frame->visible_rect().ToString();
  }
  DoDeliverFrame(std::move(delivered_video_frame), estimated_capture_time);
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverEncodedVideoFrame(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  for (const auto& callback : callbacks_) {
    callback.second.encoded_frame_callback.Run(frame, estimated_capture_time);
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::
    NewSubCaptureTargetVersionOnVideoTaskRunner(
        uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  for (const auto& callback : callbacks_) {
    callback.second.sub_capture_target_version_callback.Run(
        sub_capture_target_version);
  }
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::SettingsMatch(
    const VideoTrackAdapterSettings& settings) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  return settings_ == settings;
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  return callbacks_.empty();
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DoDeliverFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  if (callbacks_.empty()) {
    OnFrameDropped(
        media::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks);
  }
  for (const auto& callback : callbacks_) {
    MaybeUpdateTrackSettings(callback.second.settings_callback, *video_frame);
    callback.second.frame_callback.Run(video_frame, estimated_capture_time);
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  // Notify callbacks, such as
  // MediaStreamVideoTrack::FrameDeliverer::NotifyFrameDroppedOnVideoTaskRunner.
  for (const auto& callback : callbacks_) {
    callback.second.notify_frame_dropped_callback.Run(reason);
  }
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeDropFrame(
    const media::VideoFrame& frame,
    float source_frame_rate,
    media::VideoCaptureFrameDropReason* reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);

  // Never drop frames if the max frame rate has not been specified.
  if (!settings_.max_frame_rate().has_value()) {
    timestamp_last_delivered_frame_ = frame.timestamp();
    return false;
  }

  const base::TimeDelta delta =
      (frame.timestamp() - timestamp_last_delivered_frame_);

  // Keep the frame if the time since the last frame is completely off.
  if (delta.is_negative() || delta > kMaxTimeBetweenFrames) {
    // Reset |timestamp_last_delivered_frame_| and |accumulated_drift|.
    timestamp_last_delivered_frame_ = frame.timestamp();
    accumulated_drift_ = base::Milliseconds(0.0);
    return false;
  }

  DCHECK(target_delta_ && max_delta_deviation_);
  if (delta < target_delta_.value() - max_delta_deviation_.value() -
                  accumulated_drift_) {
    // Drop the frame because the input frame rate is too high.
    *reason = media::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested;
    return true;
  }

  // Keep the frame and store the accumulated drift.
  timestamp_last_delivered_frame_ = frame.timestamp();
  accumulated_drift_ += delta - target_delta_.value();
  DCHECK_GE(accumulated_drift_, -max_delta_deviation_.value());
  // Limit the maximum accumulated drift to half of the target delta. If we
  // don't do this, it may happen that we output a series of frames too quickly
  // after a period of no frames. There is no need to actively limit the minimum
  // accumulated drift because that happens automatically when we drop frames
  // that are too close in time.
  accumulated_drift_ = std::min(accumulated_drift_, target_delta_.value() / 2);
  return false;
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeUpdateTrackSettings(
    const VideoTrackSettingsInternalCallback& settings_callback,
    const media::VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  ComputeFrameRate(frame.timestamp(), &track_settings_.frame_rate,
                   &track_settings_.prev_frame_timestamp);
  if (MaybeUpdateFrameRate(&track_settings_) ||
      frame.natural_size() != track_settings_.frame_size) {
    track_settings_.frame_size = frame.natural_size();
    settings_callback.Run(track_settings_.frame_size,
                          track_settings_.frame_rate);
  }
}
void VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeUpdateTracksFormat(
    const media::VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  if (MaybeUpdateFrameRate(&source_format_settings_) ||
      frame.natural_size() != track_settings_.frame_size) {
    source_format_settings_.frame_size = frame.natural_size();
    media::VideoCaptureFormat source_format;
    source_format.frame_size = source_format_settings_.frame_size;
    source_format.frame_rate = source_format_settings_.frame_rate;
    for (const auto& callback : callbacks_)
      callback.second.format_callback.Run(source_format);
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::ResetFrameRate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_sequence_checker_);
  for (const auto& callback : callbacks_) {
    callback.second.settings_callback.Run(track_settings_.frame_size, 0.0);
  }
}

VideoTrackAdapter::VideoTrackAdapter(
    scoped_refptr<base::SequencedTaskRunner> video_task_runner,
    base::WeakPtr<MediaStreamVideoSource> media_stream_video_source)
    : video_task_runner_(video_task_runner),
      media_stream_video_source_(media_stream_video_source),
      renderer_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      muted_state_(false),
      frame_counter_(0),
      old_frame_counter_snapshot_(0),
      source_frame_rate_(0.0f) {
  DCHECK(video_task_runner);
}

VideoTrackAdapter::~VideoTrackAdapter() {
  DCHECK(adapters_.empty());
  DCHECK(!monitoring_frame_rate_timer_);
}

void VideoTrackAdapter::AddTrack(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameCB frame_callback,
    VideoCaptureNotifyFrameDroppedCB notify_frame_dropped_callback,
    EncodedVideoFrameCB encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback,
    VideoTrackSettingsCallback settings_callback,
    VideoTrackFormatCallback format_callback,
    const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::AddTrackOnVideoTaskRunner,
          WTF::CrossThreadUnretained(this), WTF::CrossThreadUnretained(track),
          CrossThreadBindRepeating(std::move(frame_callback)),
          CrossThreadBindRepeating(std::move(notify_frame_dropped_callback)),
          CrossThreadBindRepeating(std::move(encoded_frame_callback)),
          CrossThreadBindRepeating(
              std::move(sub_capture_target_version_callback)),
          CrossThreadBindRepeating(std::move(settings_callback)),
          CrossThreadBindRepeating(std::move(format_callback)), settings));
}

void VideoTrackAdapter::AddTrackOnVideoTaskRunner(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameInternalCallback frame_callback,
    VideoCaptureNotifyFrameDroppedInternalCallback
        notify_frame_dropped_callback,
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionInternalCallback
        sub_capture_target_version_callback,
    VideoTrackSettingsInternalCallback settings_callback,
    VideoTrackFormatInternalCallback format_callback,
    const VideoTrackAdapterSettings& settings) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<VideoFrameResolutionAdapter> adapter;
  for (const auto& frame_adapter : adapters_) {
    if (frame_adapter->SettingsMatch(settings)) {
      adapter = frame_adapter.get();
      break;
    }
  }
  if (!adapter.get()) {
    adapter = base::MakeRefCounted<VideoFrameResolutionAdapter>(
        renderer_task_runner_, settings, media_stream_video_source_);
    adapters_.push_back(adapter);
  }

  adapter->AddCallbacks(track, std::move(frame_callback),
                        std::move(notify_frame_dropped_callback),
                        std::move(encoded_frame_callback),
                        std::move(sub_capture_target_version_callback),
                        std::move(settings_callback),
                        std::move(format_callback));
}

void VideoTrackAdapter::RemoveTrack(const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::RemoveTrackOnVideoTaskRunner,
                          WrapRefCounted(this), CrossThreadUnretained(track)));
}

void VideoTrackAdapter::ReconfigureTrack(
    const MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::ReconfigureTrackOnVideoTaskRunner,
                          WrapRefCounted(this), CrossThreadUnretained(track),
                          settings));
}

void VideoTrackAdapter::StartFrameMonitoring(
    double source_frame_rate,
    const OnMutedCallback& on_muted_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VideoTrackAdapter::OnMutedCallback bound_on_muted_callback =
      base::BindPostTaskToCurrentDefault(on_muted_callback);

  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::StartFrameMonitoringOnVideoTaskRunner,
          WrapRefCounted(this),
          CrossThreadBindRepeating(std::move(bound_on_muted_callback)),
          source_frame_rate));
}

void VideoTrackAdapter::StopFrameMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::StopFrameMonitoringOnVideoTaskRunner,
          WrapRefCounted(this)));
}

void VideoTrackAdapter::SetSourceFrameSize(const gfx::Size& source_frame_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::SetSourceFrameSizeOnVideoTaskRunner,
          WrapRefCounted(this), source_frame_size));
}

bool VideoTrackAdapter::CalculateDesiredSize(
    bool is_rotated,
    const gfx::Size& original_input_size,
    const VideoTrackAdapterSettings& settings,
    gfx::Size* desired_size) {
  // Perform all the rescaling computations as if the device was never rotated.
  int width =
      is_rotated ? original_input_size.height() : original_input_size.width();
  int height =
      is_rotated ? original_input_size.width() : original_input_size.height();
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);

  // Rescale only if a target size was provided in |settings|.
  if (settings.target_size()) {
    // Adjust the size of the frame to the maximum allowed size.
    width =
        ClampToValidDimension(std::min(width, settings.target_size()->width()));
    height = ClampToValidDimension(
        std::min(height, settings.target_size()->height()));

    // If the area of the frame is zero, ignore aspect-ratio correction.
    if (width * height > 0) {
      double ratio = static_cast<double>(width) / height;
      DCHECK(std::isfinite(ratio));
      if (ratio > settings.max_aspect_ratio() ||
          ratio < settings.min_aspect_ratio()) {
        // Make sure |min_aspect_ratio| <= |desired_ratio| <=
        // |max_aspect_ratio|.
        double desired_ratio =
            std::max(std::min(ratio, settings.max_aspect_ratio()),
                     settings.min_aspect_ratio());
        DCHECK(std::isfinite(desired_ratio));
        DCHECK_NE(desired_ratio, 0.0);

        if (ratio < desired_ratio) {
          double desired_height_fp = (height * ratio) / desired_ratio;
          DCHECK(std::isfinite(desired_height_fp));
          height = static_cast<int>(desired_height_fp);
          // Make sure we scale to an even height to avoid rounding errors
          height = (height + 1) & ~1;
        } else if (ratio > desired_ratio) {
          double desired_width_fp = (width * desired_ratio) / ratio;
          DCHECK(std::isfinite(desired_width_fp));
          width = static_cast<int>(desired_width_fp);
          // Make sure we scale to an even width to avoid rounding errors.
          width = (width + 1) & ~1;
        }
      }
    }
  } else if (width > media::limits::kMaxDimension ||
             height > media::limits::kMaxDimension) {
    return false;
  }

  // Output back taking device rotation into account.
  *desired_size =
      is_rotated ? gfx::Size(height, width) : gfx::Size(width, height);
  return true;
}

void VideoTrackAdapter::StartFrameMonitoringOnVideoTaskRunner(
    OnMutedInternalCallback on_muted_callback,
    double source_frame_rate) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!monitoring_frame_rate_timer_);

  on_muted_callback_ = std::move(on_muted_callback);
  monitoring_frame_rate_timer_ = std::make_unique<LowPrecisionTimer>(
      video_task_runner_,
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &VideoTrackAdapter::CheckFramesReceivedOnVideoTaskRunner,
          WrapRefCounted(this))));

  // If the source does not know the frame rate, set one by default.
  if (source_frame_rate == 0.0f)
    source_frame_rate = MediaStreamVideoSource::kDefaultFrameRate;
  source_frame_rate_ = source_frame_rate;
  DVLOG(1) << "Monitoring frame creation, first (large) delay: "
           << (kFirstFrameTimeoutInFrameIntervals / source_frame_rate_) << "s";
  old_frame_counter_snapshot_ = frame_counter_;
  monitoring_frame_rate_timer_->StartOneShot(
      base::Seconds(kFirstFrameTimeoutInFrameIntervals / source_frame_rate_));
}

void VideoTrackAdapter::StopFrameMonitoringOnVideoTaskRunner() {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  if (!monitoring_frame_rate_timer_) {
    // Already stopped.
    return;
  }
  monitoring_frame_rate_timer_->Shutdown();
  monitoring_frame_rate_timer_.reset();
  on_muted_callback_ = OnMutedInternalCallback();
}

void VideoTrackAdapter::SetSourceFrameSizeOnVideoTaskRunner(
    const gfx::Size& source_frame_size) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  source_frame_size_ = source_frame_size;
}

void VideoTrackAdapter::RemoveTrackOnVideoTaskRunner(
    const MediaStreamVideoTrack* track) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
    (*it)->RemoveCallbacks(track);
    if ((*it)->IsEmpty()) {
      adapters_.erase(it);
      break;
    }
  }
}

void VideoTrackAdapter::ReconfigureTrackOnVideoTaskRunner(
    const MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& settings) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());

  VideoFrameResolutionAdapter::VideoTrackCallbacks track_callbacks;
  // Remove the track.
  for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
    track_callbacks = (*it)->RemoveAndGetCallbacks(track);
    if (!track_callbacks.frame_callback)
      continue;
    if ((*it)->IsEmpty()) {
      DCHECK(track_callbacks.frame_callback);
      adapters_.erase(it);
    }
    break;
  }

  // If the track was found, re-add it with new settings.
  if (track_callbacks.frame_callback) {
    AddTrackOnVideoTaskRunner(
        track, std::move(track_callbacks.frame_callback),
        std::move(track_callbacks.notify_frame_dropped_callback),
        std::move(track_callbacks.encoded_frame_callback),
        std::move(track_callbacks.sub_capture_target_version_callback),
        std::move(track_callbacks.settings_callback),
        std::move(track_callbacks.format_callback), settings);
  }
}

void VideoTrackAdapter::DeliverFrameOnVideoTaskRunner(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("media", "VideoTrackAdapter::DeliverFrameOnVideoTaskRunner");
  ++frame_counter_;

  bool is_device_rotated = false;
  // TODO(guidou): Use actual device information instead of this heuristic to
  // detect frames from rotated devices. https://crbug.com/722748
  if (source_frame_size_ &&
      video_frame->natural_size().width() == source_frame_size_->height() &&
      video_frame->natural_size().height() == source_frame_size_->width()) {
    is_device_rotated = true;
  }
  for (const auto& adapter : adapters_) {
    adapter->DeliverFrame(video_frame, estimated_capture_time,
                          is_device_rotated);
  }
}

void VideoTrackAdapter::DeliverEncodedVideoFrameOnVideoTaskRunner(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("media",
               "VideoTrackAdapter::DeliverEncodedVideoFrameOnVideoTaskRunner");
  for (const auto& adapter : adapters_)
    adapter->DeliverEncodedVideoFrame(frame, estimated_capture_time);
}

void VideoTrackAdapter::OnFrameDroppedOnVideoTaskRunner(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("media", "VideoTrackAdapter::OnFrameDroppedOnVideoTaskRunner");
  for (const auto& adapter : adapters_) {
    adapter->OnFrameDropped(reason);
  }
}

void VideoTrackAdapter::NewSubCaptureTargetVersionOnVideoTaskRunner(
    uint32_t sub_capture_target_version) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0(
      "media",
      "VideoTrackAdapter::NewSubCaptureTargetVersionOnVideoTaskRunner");
  for (const auto& adapter : adapters_) {
    adapter->NewSubCaptureTargetVersionOnVideoTaskRunner(
        sub_capture_target_version);
  }
}

void VideoTrackAdapter::CheckFramesReceivedOnVideoTaskRunner() {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());

  DVLOG_IF(1, old_frame_counter_snapshot_ == frame_counter_)
      << "No frames have passed, setting source as Muted.";
  bool muted_state = old_frame_counter_snapshot_ == frame_counter_;
  if (muted_state_ != muted_state) {
    on_muted_callback_.Run(muted_state);
    muted_state_ = muted_state;
    if (muted_state_) {
      for (const auto& adapter : adapters_)
        adapter->ResetFrameRate();
    }
  }

  old_frame_counter_snapshot_ = frame_counter_;
  monitoring_frame_rate_timer_->StartOneShot(
      base::Seconds(kNormalFrameTimeoutInFrameIntervals / source_frame_rate_));
}

}  // namespace blink
