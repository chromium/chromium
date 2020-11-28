// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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

// Min delta time between two frames allowed without being dropped if a max
// frame rate is specified.
const double kMinTimeInMsBetweenFrames = 5;
// If the delta between two frames is bigger than this, we will consider it to
// be invalid and reset the fps calculation.
const double kMaxTimeInMsBetweenFrames = 1000;

const double kFrameRateChangeIntervalInSeconds = 1;
const double kFrameRateChangeRate = 0.01;
const double kFrameRateUpdateIntervalInSeconds = 5;

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
    if ((now - settings->new_frame_rate_timestamp).InSecondsF() >
        kFrameRateChangeIntervalInSeconds) {
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
  if ((now - settings->last_update_timestamp).InSecondsF() >
      kFrameRateUpdateIntervalInSeconds) {
    settings->last_update_timestamp = now;
    settings->last_updated_frame_rate = settings->frame_rate;
    return true;
  }
  return false;
}

}  // anonymous namespace

// VideoFrameResolutionAdapter is created on and lives on the IO-thread. It does
// the resolution adaptation and delivers frames to all registered tracks on the
// IO-thread. All method calls must be on the IO-thread.
class VideoTrackAdapter::VideoFrameResolutionAdapter
    : public WTF::ThreadSafeRefCounted<VideoFrameResolutionAdapter> {
 public:
  struct VideoTrackCallbacks {
    VideoCaptureDeliverFrameInternalCallback frame_callback;
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback;
    VideoTrackSettingsInternalCallback settings_callback;
    VideoTrackFormatInternalCallback format_callback;
  };
  // Setting |max_frame_rate| to 0.0, means that no frame rate limitation
  // will be done.
  VideoFrameResolutionAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
      const VideoTrackAdapterSettings& settings,
      base::WeakPtr<MediaStreamVideoSource> media_stream_video_source);

  // Add |frame_callback|, |encoded_frame_callback| to receive video frames on
  // the IO-thread and |settings_callback| to set track settings on the main
  // thread. |frame_callback| will however be released on the main render
  // thread.
  void AddCallbacks(
      const MediaStreamVideoTrack* track,
      VideoCaptureDeliverFrameInternalCallback frame_callback,
      DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
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

  void DeliverFrame(scoped_refptr<media::VideoFrame> frame,
                    const base::TimeTicks& estimated_capture_time,
                    bool is_device_rotated);

  void DeliverEncodedVideoFrame(scoped_refptr<EncodedVideoFrame> frame,
                                base::TimeTicks estimated_capture_time);

  // Returns true if all arguments match with the output of this adapter.
  bool SettingsMatch(const VideoTrackAdapterSettings& settings) const;

  bool IsEmpty() const;

  // Sets frame rate to 0.0 if frame monitor has detected muted state.
  void ResetFrameRate();

 private:
  virtual ~VideoFrameResolutionAdapter();
  friend class WTF::ThreadSafeRefCounted<VideoFrameResolutionAdapter>;

  void DoDeliverFrame(scoped_refptr<media::VideoFrame> frame,
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

  void PostFrameDroppedToMainTaskRunner(
      media::VideoCaptureFrameDropReason reason);

  // Bound to the IO-thread.
  THREAD_CHECKER(io_thread_checker_);

  // The task runner where we will release VideoCaptureDeliverFrameCB
  // registered in AddCallbacks.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  base::WeakPtr<MediaStreamVideoSource> media_stream_video_source_;

  VideoTrackAdapterSettings settings_;
  double frame_rate_;
  base::TimeDelta last_time_stamp_;
  double keep_frame_counter_;

  ComputedSettings track_settings_;
  ComputedSettings source_format_settings_;

  base::flat_map<const MediaStreamVideoTrack*, VideoTrackCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResolutionAdapter);
};

VideoTrackAdapter::VideoFrameResolutionAdapter::VideoFrameResolutionAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
    const VideoTrackAdapterSettings& settings,
    base::WeakPtr<MediaStreamVideoSource> media_stream_video_source)
    : renderer_task_runner_(render_message_loop),
      media_stream_video_source_(media_stream_video_source),
      settings_(settings),
      frame_rate_(MediaStreamVideoSource::kDefaultFrameRate),
      last_time_stamp_(base::TimeDelta::Max()),
      keep_frame_counter_(0.0) {
  DCHECK(renderer_task_runner_.get());
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  CHECK_NE(0, settings_.max_aspect_ratio());

  base::Optional<double> max_fps_override =
      Platform::Current()->GetWebRtcMaxCaptureFrameRate();
  if (max_fps_override) {
    DVLOG(1) << "Overriding max frame rate.  Was=" << settings_.max_frame_rate()
             << ", Now=" << *max_fps_override;
    settings_.set_max_frame_rate(*max_fps_override);
  }
}

VideoTrackAdapter::VideoFrameResolutionAdapter::~VideoFrameResolutionAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  DCHECK(callbacks_.empty());
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::AddCallbacks(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameInternalCallback frame_callback,
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
    VideoTrackSettingsInternalCallback settings_callback,
    VideoTrackFormatInternalCallback format_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  VideoTrackCallbacks track_callbacks = {
      std::move(frame_callback), std::move(encoded_frame_callback),
      std::move(settings_callback), std::move(format_callback)};
  callbacks_.emplace(track, std::move(track_callbacks));
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveCallbacks(
    const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  callbacks_.erase(track);
}

VideoTrackAdapter::VideoFrameResolutionAdapter::VideoTrackCallbacks
VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveAndGetCallbacks(
    const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  VideoTrackCallbacks track_callbacks;
  auto it = callbacks_.find(track);
  if (it == callbacks_.end())
    return track_callbacks;

  track_callbacks = std::move(it->second);
  callbacks_.erase(it);
  return track_callbacks;
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverFrame(
    scoped_refptr<media::VideoFrame> frame,
    const base::TimeTicks& estimated_capture_time,
    bool is_device_rotated) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  if (!frame) {
    DLOG(ERROR) << "Incoming frame is not valid.";
    PostFrameDroppedToMainTaskRunner(
        media::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid);
    return;
  }

  ComputeFrameRate(frame->timestamp(), &source_format_settings_.frame_rate,
                   &source_format_settings_.prev_frame_timestamp);
  MaybeUpdateTracksFormat(*frame);

  double frame_rate = frame->metadata()->frame_rate.value_or(
      MediaStreamVideoSource::kUnknownFrameRate);

  auto frame_drop_reason = media::VideoCaptureFrameDropReason::kNone;
  if (MaybeDropFrame(*frame, frame_rate, &frame_drop_reason)) {
    PostFrameDroppedToMainTaskRunner(frame_drop_reason);
    return;
  }

  // If the frame is a texture not backed up by GPU memory we don't apply
  // cropping/scaling and deliver the frame as-is, leaving it up to the
  // destination to rescale it. Otherwise, cropping and scaling is soft-applied
  // before delivery for efficiency.
  //
  // TODO(crbug.com/362521): Allow cropping/scaling of non-GPU memory backed
  // textures.
  if (frame->HasTextures() &&
      frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DoDeliverFrame(std::move(frame), estimated_capture_time);
    return;
  }
  scoped_refptr<media::VideoFrame> video_frame(frame);

  gfx::Size desired_size;
  CalculateDesiredSize(is_device_rotated, frame->natural_size(), settings_,
                       &desired_size);
  if (desired_size != frame->natural_size()) {
    // Get the largest centered rectangle with the same aspect ratio of
    // |desired_size| that fits entirely inside of |frame->visible_rect()|.
    // This will be the rect we need to crop the original frame to.
    // From this rect, the original frame can be scaled down to |desired_size|.
    gfx::Rect region_in_frame =
        media::ComputeLetterboxRegion(frame->visible_rect(), desired_size);

    if (frame->HasTextures() || frame->HasGpuMemoryBuffer()) {
      // ComputeLetterboxRegion() produces in some cases odd dimensions due to
      // internal rounding errors; |region_in_frame| is always smaller or equal
      // to frame->visible_rect(), we can "grow it" if the dimensions are odd.
      region_in_frame.set_width((region_in_frame.width() + 1) & ~1);
      region_in_frame.set_height((region_in_frame.height() + 1) & ~1);
    }

    video_frame = media::VideoFrame::WrapVideoFrame(
        frame, frame->format(), region_in_frame, desired_size);
    if (!video_frame) {
      PostFrameDroppedToMainTaskRunner(
          media::VideoCaptureFrameDropReason::
              kResolutionAdapterWrappingFrameForCroppingFailed);
      return;
    }

    DVLOG(3) << "desired size  " << desired_size.ToString()
             << " output natural size "
             << video_frame->natural_size().ToString()
             << " output visible rect  "
             << video_frame->visible_rect().ToString();
  }
  DoDeliverFrame(std::move(video_frame), estimated_capture_time);
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverEncodedVideoFrame(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  for (const auto& callback : callbacks_) {
    callback.second.encoded_frame_callback.Run(frame, estimated_capture_time);
  }
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::SettingsMatch(
    const VideoTrackAdapterSettings& settings) const {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  return settings_ == settings;
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  return callbacks_.empty();
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DoDeliverFrame(
    scoped_refptr<media::VideoFrame> frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (callbacks_.empty()) {
    PostFrameDroppedToMainTaskRunner(
        media::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks);
  }
  for (const auto& callback : callbacks_) {
    MaybeUpdateTrackSettings(callback.second.settings_callback, *frame);
    callback.second.frame_callback.Run(frame, estimated_capture_time);
  }
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeDropFrame(
    const media::VideoFrame& frame,
    float source_frame_rate,
    media::VideoCaptureFrameDropReason* reason) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  // Do not drop frames if max frame rate hasn't been specified.
  if (settings_.max_frame_rate() == 0.0f ||
      (base::FeatureList::IsEnabled(
           features::kMediaStreamTrackUseConfigMaxFrameRate) &&
       source_frame_rate > 0 &&
       source_frame_rate <= settings_.max_frame_rate())) {
    last_time_stamp_ = frame.timestamp();
    return false;
  }

  const double delta_ms =
      (frame.timestamp() - last_time_stamp_).InMillisecondsF();

  // Check if the time since the last frame is completely off.
  if (delta_ms < 0 || delta_ms > kMaxTimeInMsBetweenFrames) {
    // Reset |last_time_stamp_| and fps calculation.
    last_time_stamp_ = frame.timestamp();
    frame_rate_ = MediaStreamVideoSource::kDefaultFrameRate;
    keep_frame_counter_ = 0.0;
    return false;
  }

  if (delta_ms < kMinTimeInMsBetweenFrames) {
    // We have seen video frames being delivered from camera devices back to
    // back. The simple AR filter for frame rate calculation is too short to
    // handle that. https://crbug/394315
    // TODO(perkj): Can we come up with a way to fix the times stamps and the
    // timing when frames are delivered so all frames can be used?
    // The time stamps are generated by Chrome and not the actual device.
    // Most likely the back to back problem is caused by software and not the
    // actual camera.
    DVLOG(3) << "Drop frame since delta time since previous frame is "
             << delta_ms << "ms.";
    *reason = media::VideoCaptureFrameDropReason::
        kResolutionAdapterTimestampTooCloseToPrevious;
    return true;
  }
  last_time_stamp_ = frame.timestamp();
  // Calculate the frame rate using a simple AR filter.
  // Use a simple filter with 0.1 weight of the current sample.
  frame_rate_ = 100 / delta_ms + 0.9 * frame_rate_;

  // Prefer to not drop frames.
  if (settings_.max_frame_rate() + 0.5f > frame_rate_)
    return false;  // Keep this frame.

  // The input frame rate is higher than requested.
  // Decide if we should keep this frame or drop it.
  keep_frame_counter_ += settings_.max_frame_rate() / frame_rate_;
  if (keep_frame_counter_ >= 1) {
    keep_frame_counter_ -= 1;
    // Keep the frame.
    return false;
  }
  DVLOG(3) << "Drop frame. Input frame_rate_ " << frame_rate_ << ".";
  *reason = media::VideoCaptureFrameDropReason::
      kResolutionAdapterFrameRateIsHigherThanRequested;
  return true;
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeUpdateTrackSettings(
    const VideoTrackSettingsInternalCallback& settings_callback,
    const media::VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  for (const auto& callback : callbacks_) {
    callback.second.settings_callback.Run(track_settings_.frame_size, 0.0);
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::
    PostFrameDroppedToMainTaskRunner(
        media::VideoCaptureFrameDropReason reason) {
  PostCrossThreadTask(
      *renderer_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&MediaStreamVideoSource::OnFrameDropped,
                          media_stream_video_source_, reason));
}

VideoTrackAdapter::VideoTrackAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<MediaStreamVideoSource> media_stream_video_source)
    : io_task_runner_(io_task_runner),
      media_stream_video_source_(media_stream_video_source),
      renderer_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      monitoring_frame_rate_(false),
      muted_state_(false),
      frame_counter_(0),
      source_frame_rate_(0.0f) {
  DCHECK(io_task_runner);
}

VideoTrackAdapter::~VideoTrackAdapter() {
  DCHECK(adapters_.IsEmpty());
}

void VideoTrackAdapter::AddTrack(const MediaStreamVideoTrack* track,
                                 VideoCaptureDeliverFrameCB frame_callback,
                                 EncodedVideoFrameCB encoded_frame_callback,
                                 VideoTrackSettingsCallback settings_callback,
                                 VideoTrackFormatCallback format_callback,
                                 const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::AddTrackOnIO, WTF::CrossThreadUnretained(this),
          WTF::CrossThreadUnretained(track),
          WTF::Passed(CrossThreadBindRepeating(std::move(frame_callback))),
          WTF::Passed(
              CrossThreadBindRepeating(std::move(encoded_frame_callback))),
          WTF::Passed(CrossThreadBindRepeating(std::move(settings_callback))),
          WTF::Passed(CrossThreadBindRepeating(std::move(format_callback))),
          settings));
}

void VideoTrackAdapter::AddTrackOnIO(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameInternalCallback frame_callback,
    DeliverEncodedVideoFrameInternalCallback encoded_frame_callback,
    VideoTrackSettingsInternalCallback settings_callback,
    VideoTrackFormatInternalCallback format_callback,
    const VideoTrackAdapterSettings& settings) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
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

  adapter->AddCallbacks(
      track, std::move(frame_callback), std::move(encoded_frame_callback),
      std::move(settings_callback), std::move(format_callback));
}

void VideoTrackAdapter::RemoveTrack(const MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::RemoveTrackOnIO,
                          WrapRefCounted(this), CrossThreadUnretained(track)));
}

void VideoTrackAdapter::ReconfigureTrack(
    const MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::ReconfigureTrackOnIO,
                          WrapRefCounted(this), CrossThreadUnretained(track),
                          settings));
}

void VideoTrackAdapter::StartFrameMonitoring(
    double source_frame_rate,
    const OnMutedCallback& on_muted_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VideoTrackAdapter::OnMutedCallback bound_on_muted_callback =
      media::BindToCurrentLoop(on_muted_callback);

  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::StartFrameMonitoringOnIO, WrapRefCounted(this),
          WTF::Passed(
              CrossThreadBindRepeating(std::move(bound_on_muted_callback))),
          source_frame_rate));
}

void VideoTrackAdapter::StopFrameMonitoring() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::StopFrameMonitoringOnIO,
                          WrapRefCounted(this)));
}

void VideoTrackAdapter::SetSourceFrameSize(const gfx::Size& source_frame_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VideoTrackAdapter::SetSourceFrameSizeOnIO,
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

void VideoTrackAdapter::StartFrameMonitoringOnIO(
    OnMutedInternalCallback on_muted_callback,
    double source_frame_rate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(!monitoring_frame_rate_);

  monitoring_frame_rate_ = true;

  // If the source does not know the frame rate, set one by default.
  if (source_frame_rate == 0.0f)
    source_frame_rate = MediaStreamVideoSource::kDefaultFrameRate;
  source_frame_rate_ = source_frame_rate;
  DVLOG(1) << "Monitoring frame creation, first (large) delay: "
           << (kFirstFrameTimeoutInFrameIntervals / source_frame_rate_) << "s";
  PostDelayedCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::CheckFramesReceivedOnIO, WrapRefCounted(this),
          WTF::Passed(std::move(on_muted_callback)), frame_counter_),
      base::TimeDelta::FromSecondsD(kFirstFrameTimeoutInFrameIntervals /
                                    source_frame_rate_));
}

void VideoTrackAdapter::StopFrameMonitoringOnIO() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  monitoring_frame_rate_ = false;
}

void VideoTrackAdapter::SetSourceFrameSizeOnIO(
    const gfx::Size& source_frame_size) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  source_frame_size_ = source_frame_size;
}

void VideoTrackAdapter::RemoveTrackOnIO(const MediaStreamVideoTrack* track) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (auto* it = adapters_.begin(); it != adapters_.end(); ++it) {
    (*it)->RemoveCallbacks(track);
    if ((*it)->IsEmpty()) {
      adapters_.erase(it);
      break;
    }
  }
}

void VideoTrackAdapter::ReconfigureTrackOnIO(
    const MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& settings) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  VideoFrameResolutionAdapter::VideoTrackCallbacks track_callbacks;
  // Remove the track.
  for (auto* it = adapters_.begin(); it != adapters_.end(); ++it) {
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
    AddTrackOnIO(track, std::move(track_callbacks.frame_callback),
                 std::move(track_callbacks.encoded_frame_callback),
                 std::move(track_callbacks.settings_callback),
                 std::move(track_callbacks.format_callback), settings);
  }
}

void VideoTrackAdapter::DeliverFrameOnIO(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "VideoTrackAdapter::DeliverFrameOnIO");
  ++frame_counter_;

  bool is_device_rotated = false;
  // TODO(guidou): Use actual device information instead of this heuristic to
  // detect frames from rotated devices. https://crbug.com/722748
  if (source_frame_size_ &&
      frame->natural_size().width() == source_frame_size_->height() &&
      frame->natural_size().height() == source_frame_size_->width()) {
    is_device_rotated = true;
  }
  if (adapters_.IsEmpty()) {
    PostCrossThreadTask(
        *renderer_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&MediaStreamVideoSource::OnFrameDropped,
                            media_stream_video_source_,
                            media::VideoCaptureFrameDropReason::
                                kVideoTrackAdapterHasNoResolutionAdapters));
  }
  for (const auto& adapter : adapters_)
    adapter->DeliverFrame(frame, estimated_capture_time, is_device_rotated);
}

void VideoTrackAdapter::DeliverEncodedVideoFrameOnIO(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "VideoTrackAdapter::DeliverEncodedVideoFrameOnIO");
  for (const auto& adapter : adapters_)
    adapter->DeliverEncodedVideoFrame(frame, estimated_capture_time);
}

void VideoTrackAdapter::CheckFramesReceivedOnIO(
    OnMutedInternalCallback set_muted_state_callback,
    uint64_t old_frame_counter_snapshot) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!monitoring_frame_rate_)
    return;

  DVLOG_IF(1, old_frame_counter_snapshot == frame_counter_)
      << "No frames have passed, setting source as Muted.";

  bool muted_state = old_frame_counter_snapshot == frame_counter_;
  if (muted_state_ != muted_state) {
    set_muted_state_callback.Run(muted_state);
    muted_state_ = muted_state;
    if (muted_state_) {
      for (const auto& adapter : adapters_)
        adapter->ResetFrameRate();
    }
  }

  PostDelayedCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &VideoTrackAdapter::CheckFramesReceivedOnIO, WrapRefCounted(this),
          WTF::Passed(std::move(set_muted_state_callback)), frame_counter_),
      base::TimeDelta::FromSecondsD(kNormalFrameTimeoutInFrameIntervals /
                                    source_frame_rate_));
}

}  // namespace blink
