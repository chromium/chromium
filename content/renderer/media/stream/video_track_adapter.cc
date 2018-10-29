// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/video_track_adapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"

namespace content {

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

// Empty method used for keeping a reference to the original media::VideoFrame
// in VideoFrameResolutionAdapter::DeliverFrame if cropping is needed.
// The reference to |frame| is kept in the closure that calls this method.
void TrackReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {}

void ResetCallbackOnMainRenderThread(
    std::unique_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}

int ClampToValidDimension(int dimension) {
  return std::min(static_cast<int>(media::limits::kMaxDimension),
                  std::max(0, dimension));
}

}  // anonymous namespace

// VideoFrameResolutionAdapter is created on and lives on the IO-thread. It does
// the resolution adaptation and delivers frames to all registered tracks on the
// IO-thread. All method calls must be on the IO-thread.
class VideoTrackAdapter::VideoFrameResolutionAdapter
    : public base::RefCountedThreadSafe<VideoFrameResolutionAdapter> {
 public:
  // Setting |max_frame_rate| to 0.0, means that no frame rate limitation
  // will be done.
  VideoFrameResolutionAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
      const VideoTrackAdapterSettings& settings);

  // Add |callback| to receive video frames on the IO-thread.
  // |callback| will however be released on the main render thread.
  void AddCallback(const MediaStreamVideoTrack* track,
                   const VideoCaptureDeliverFrameCB& callback);

  // Removes the callback associated with |track| from receiving video frames if
  // |track| has been added. It is ok to call RemoveAndReleaseCallback() even if
  // |track| has not been added. The callback is released on the main render
  // thread.
  void RemoveAndReleaseCallback(const MediaStreamVideoTrack* track);

  // Removes the callback associated with |track| from receiving video frames if
  // |track| has been added. It is ok to call RemoveAndGetCallback() even if the
  // |track| has not been added. The functions returns the callback if it was
  // removed, or a null pointer if |track| was not present in the adapter.
  std::unique_ptr<VideoCaptureDeliverFrameCB> RemoveAndGetCallback(
      const MediaStreamVideoTrack* track);

  void DeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const base::TimeTicks& estimated_capture_time,
                    bool is_device_rotated);

  // Returns true if all arguments match with the output of this adapter.
  bool SettingsMatch(const VideoTrackAdapterSettings& settings) const;

  bool IsEmpty() const;

 private:
  virtual ~VideoFrameResolutionAdapter();
  friend class base::RefCountedThreadSafe<VideoFrameResolutionAdapter>;

  void DoDeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                      const base::TimeTicks& estimated_capture_time);

  // Returns |true| if the input frame rate is higher that the requested max
  // frame rate and |frame| should be dropped.
  bool MaybeDropFrame(const scoped_refptr<media::VideoFrame>& frame,
                      float source_frame_rate);

  // Bound to the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // The task runner where we will release VideoCaptureDeliverFrameCB
  // registered in AddCallback.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  VideoTrackAdapterSettings settings_;
  double frame_rate_;
  base::TimeDelta last_time_stamp_;
  double keep_frame_counter_;

  typedef std::pair<const MediaStreamVideoTrack*, VideoCaptureDeliverFrameCB>
      VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResolutionAdapter);
};

VideoTrackAdapter::VideoFrameResolutionAdapter::VideoFrameResolutionAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
    const VideoTrackAdapterSettings& settings)
    : renderer_task_runner_(render_message_loop),
      settings_(settings),
      frame_rate_(MediaStreamVideoSource::kDefaultFrameRate),
      last_time_stamp_(base::TimeDelta::Max()),
      keep_frame_counter_(0.0) {
  DCHECK(renderer_task_runner_.get());
  DCHECK(io_thread_checker_.CalledOnValidThread());
  CHECK_NE(0, settings_.max_aspect_ratio());

  const std::string max_fps_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebRtcMaxCaptureFramerate);
  if (!max_fps_str.empty()) {
    double value;
    if (base::StringToDouble(max_fps_str, &value) && value >= 0.0) {
      DVLOG(1) << "Overriding max frame rate.  Was="
               << settings_.max_frame_rate() << ", Now=" << value;
      settings_.set_max_frame_rate(value);
    } else {
      DLOG(ERROR) << "Unable to set max fps to " << max_fps_str;
    }
  }
}

VideoTrackAdapter::
VideoFrameResolutionAdapter::~VideoFrameResolutionAdapter() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(callbacks_.empty());
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::AddCallback(
    const MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  callbacks_.push_back(std::make_pair(track, callback));
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveAndReleaseCallback(
    const MediaStreamVideoTrack* track) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  auto it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == track) {
      // Make sure the VideoCaptureDeliverFrameCB is released on the main
      // render thread since it was added on the main render thread in
      // VideoTrackAdapter::AddTrack.
      std::unique_ptr<VideoCaptureDeliverFrameCB> callback(
          new VideoCaptureDeliverFrameCB(it->second));
      callbacks_.erase(it);
      renderer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ResetCallbackOnMainRenderThread,
                                    std::move(callback)));

      return;
    }
  }
}

std::unique_ptr<VideoCaptureDeliverFrameCB>
VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveAndGetCallback(
    const MediaStreamVideoTrack* track) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  std::unique_ptr<VideoCaptureDeliverFrameCB> ret;
  for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
    if (it->first == track) {
      // Make sure the VideoCaptureDeliverFrameCB is released on the main
      // render thread since it was added on the main render thread in
      // VideoTrackAdapter::AddTrack.
      ret = std::make_unique<VideoCaptureDeliverFrameCB>(it->second);
      callbacks_.erase(it);
      break;
    }
  }
  return ret;
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const base::TimeTicks& estimated_capture_time,
    bool is_device_rotated) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  if (!frame) {
    DLOG(ERROR) << "Incoming frame is not valid.";
    return;
  }

  double frame_rate;
  if (!frame->metadata()->GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                    &frame_rate)) {
    frame_rate = MediaStreamVideoSource::kUnknownFrameRate;
  }

  if (MaybeDropFrame(frame, frame_rate))
    return;

  // TODO(perkj): Allow cropping / scaling of textures once
  // http://crbug/362521 is fixed.
  if (frame->HasTextures()) {
    DoDeliverFrame(frame, estimated_capture_time);
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
    const gfx::Rect region_in_frame =
        media::ComputeLetterboxRegion(frame->visible_rect(), desired_size);

    video_frame = media::VideoFrame::WrapVideoFrame(
        frame, frame->format(), region_in_frame, desired_size);
    if (!video_frame)
      return;
    video_frame->AddDestructionObserver(
        base::BindOnce(&TrackReleaseOriginalFrame, frame));

    DVLOG(3) << "desired size  " << desired_size.ToString()
             << " output natural size "
             << video_frame->natural_size().ToString()
             << " output visible rect  "
             << video_frame->visible_rect().ToString();
  }
  DoDeliverFrame(video_frame, estimated_capture_time);
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::SettingsMatch(
    const VideoTrackAdapterSettings& settings) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return settings_ == settings;
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::IsEmpty() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return callbacks_.empty();
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DoDeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (const auto& callback : callbacks_)
    callback.second.Run(frame, estimated_capture_time);
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeDropFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    float source_frame_rate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Do not drop frames if max frame rate hasn't been specified or the source
  // frame rate is known and is lower than max.
  if (settings_.max_frame_rate() == 0.0f ||
      (source_frame_rate > 0 &&
       source_frame_rate <= settings_.max_frame_rate())) {
    return false;
  }

  const double delta_ms =
      (frame->timestamp() - last_time_stamp_).InMillisecondsF();

  // Check if the time since the last frame is completely off.
  if (delta_ms < 0 || delta_ms > kMaxTimeInMsBetweenFrames) {
    // Reset |last_time_stamp_| and fps calculation.
    last_time_stamp_ = frame->timestamp();
    frame_rate_ = MediaStreamVideoSource::kDefaultFrameRate;
    keep_frame_counter_ = 0.0;
    return false;
  }

  if (delta_ms < kMinTimeInMsBetweenFrames) {
    // We have seen video frames being delivered from camera devices back to
    // back. The simple AR filter for frame rate calculation is too short to
    // handle that. http://crbug/394315
    // TODO(perkj): Can we come up with a way to fix the times stamps and the
    // timing when frames are delivered so all frames can be used?
    // The time stamps are generated by Chrome and not the actual device.
    // Most likely the back to back problem is caused by software and not the
    // actual camera.
    DVLOG(3) << "Drop frame since delta time since previous frame is "
             << delta_ms << "ms.";
    return true;
  }
  last_time_stamp_ = frame->timestamp();
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
  return true;
}

VideoTrackAdapterSettings::VideoTrackAdapterSettings()
    : VideoTrackAdapterSettings(base::nullopt,
                                0.0,
                                std::numeric_limits<double>::max(),
                                0.0) {}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    const gfx::Size& target_size,
    double max_frame_rate)
    : VideoTrackAdapterSettings(target_size, 0.0, HUGE_VAL, max_frame_rate) {}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    base::Optional<gfx::Size> target_size,
    double min_aspect_ratio,
    double max_aspect_ratio,
    double max_frame_rate)
    : target_size_(std::move(target_size)),
      min_aspect_ratio_(min_aspect_ratio),
      max_aspect_ratio_(max_aspect_ratio),
      max_frame_rate_(max_frame_rate) {
  DCHECK(!target_size_ ||
         (target_size_->width() >= 0 && target_size_->height() >= 0));
  DCHECK(!std::isnan(min_aspect_ratio_));
  DCHECK_GE(min_aspect_ratio_, 0.0);
  DCHECK(!std::isnan(max_aspect_ratio_));
  DCHECK_GE(max_aspect_ratio_, min_aspect_ratio_);
  DCHECK(!std::isnan(max_frame_rate_));
  DCHECK_GE(max_frame_rate_, 0.0);
}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    const VideoTrackAdapterSettings& other) = default;
VideoTrackAdapterSettings& VideoTrackAdapterSettings::operator=(
    const VideoTrackAdapterSettings& other) = default;

bool VideoTrackAdapterSettings::operator==(
    const VideoTrackAdapterSettings& other) const {
  return target_size_ == other.target_size_ &&
         min_aspect_ratio_ == other.min_aspect_ratio_ &&
         max_aspect_ratio_ == other.max_aspect_ratio_ &&
         max_frame_rate_ == other.max_frame_rate_;
}

VideoTrackAdapter::VideoTrackAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      renderer_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      monitoring_frame_rate_(false),
      muted_state_(false),
      frame_counter_(0),
      source_frame_rate_(0.0f) {
  DCHECK(io_task_runner);
}

VideoTrackAdapter::~VideoTrackAdapter() {
  DCHECK(adapters_.empty());
}

void VideoTrackAdapter::AddTrack(const MediaStreamVideoTrack* track,
                                 VideoCaptureDeliverFrameCB frame_callback,
                                 const VideoTrackAdapterSettings& settings) {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoTrackAdapter::AddTrackOnIO, this, track,
                                std::move(frame_callback), settings));
}

void VideoTrackAdapter::AddTrackOnIO(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameCB frame_callback,
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
    adapter = new VideoFrameResolutionAdapter(renderer_task_runner_, settings);
    adapters_.push_back(adapter);
  }

  adapter->AddCallback(track, std::move(frame_callback));
}

void VideoTrackAdapter::RemoveTrack(const MediaStreamVideoTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoTrackAdapter::RemoveTrackOnIO, this, track));
}

void VideoTrackAdapter::ReconfigureTrack(
    const MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& settings) {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoTrackAdapter::ReconfigureTrackOnIO, this,
                                track, settings));
}

void VideoTrackAdapter::StartFrameMonitoring(
    double source_frame_rate,
    const OnMutedCallback& on_muted_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VideoTrackAdapter::OnMutedCallback bound_on_muted_callback =
      media::BindToCurrentLoop(on_muted_callback);

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoTrackAdapter::StartFrameMonitoringOnIO, this,
                     std::move(bound_on_muted_callback), source_frame_rate));
}

void VideoTrackAdapter::StopFrameMonitoring() {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoTrackAdapter::StopFrameMonitoringOnIO, this));
}

void VideoTrackAdapter::SetSourceFrameSize(const gfx::Size& source_frame_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoTrackAdapter::SetSourceFrameSizeOnIO,
                                this, source_frame_size));
}

// static
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
    const OnMutedCallback& on_muted_callback,
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
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VideoTrackAdapter::CheckFramesReceivedOnIO, this,
                     on_muted_callback, frame_counter_),
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
  for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
    (*it)->RemoveAndReleaseCallback(track);
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

  std::unique_ptr<VideoCaptureDeliverFrameCB> track_frame_callback;
  // Remove the track.
  for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
    track_frame_callback = (*it)->RemoveAndGetCallback(track);
    if ((*it)->IsEmpty()) {
      DCHECK(track_frame_callback);
      adapters_.erase(it);
      break;
    }
  }

  // If the track was found, re-add it with new settings.
  if (track_frame_callback)
    AddTrackOnIO(track, std::move(*track_frame_callback), settings);
}

void VideoTrackAdapter::DeliverFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "VideoTrackAdapter::DeliverFrameOnIO");
  ++frame_counter_;

  bool is_device_rotated = false;
  // TODO(guidou): Use actual device information instead of this heuristic to
  // detect frames from rotated devices. http://crbug.com/722748
  if (source_frame_size_ &&
      frame->natural_size().width() == source_frame_size_->height() &&
      frame->natural_size().height() == source_frame_size_->width()) {
    is_device_rotated = true;
  }
  for (const auto& adapter : adapters_)
    adapter->DeliverFrame(frame, estimated_capture_time, is_device_rotated);
}

void VideoTrackAdapter::CheckFramesReceivedOnIO(
    const OnMutedCallback& set_muted_state_callback,
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
  }

  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VideoTrackAdapter::CheckFramesReceivedOnIO, this,
                     set_muted_state_callback, frame_counter_),
      base::TimeDelta::FromSecondsD(kNormalFrameTimeoutInFrameIntervals /
                                    source_frame_rate_));
}

}  // namespace content
