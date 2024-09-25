// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/device_service.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_util.h"
#include "media/capture/content/capture_resolution_chooser.h"
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/webrtc/modules/desktop_capture/cropped_desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/cropping_window_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/fake_desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "ui/gfx/icc_profile.h"

namespace content {

namespace {

// Maximum CPU time percentage of a single core that can be consumed for desktop
// capturing. This means that on systems where screen scraping is slow we may
// need to capture at frame rate lower than requested. This is necessary to keep
// UI responsive.
const int kDefaultMaximumCpuConsumptionPercentage = 50;

// Constant which sets the cutoff frequency in an an exponential moving average
// (EMA) filter used to calculate the current frame rate (in frames per second).
constexpr float kAlpha = 0.1;

const char* DesktopMediaTypeToString(DesktopMediaID::Type type) {
  switch (type) {
    case DesktopMediaID::TYPE_NONE:
      return "NONE";
    case DesktopMediaID::TYPE_SCREEN:
      return "SCREEN";
    case DesktopMediaID::TYPE_WINDOW:
      return "WINDOW";
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      return "WEB_CONTENTS";
    default:
      return "UNKNOWN";
  }
}

webrtc::DesktopRect ComputeLetterboxRect(
    const webrtc::DesktopSize& max_size,
    const webrtc::DesktopSize& source_size) {
  gfx::Rect result = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, max_size.width(), max_size.height()),
      gfx::Size(source_size.width(), source_size.height()));
  return webrtc::DesktopRect::MakeLTRB(
      result.x(), result.y(), result.right(), result.bottom());
}

bool IsFrameUnpackedOrInverted(webrtc::DesktopFrame* frame) {
  return frame->stride() !=
      frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel;
}

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

void LogDesktopCaptureZeroHzIsActive(DesktopMediaID::Type capturer_type,
                                     bool zero_hz_is_active) {
  if (capturer_type == DesktopMediaID::TYPE_SCREEN) {
    UMA_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.IsZeroHzActive.Screen",
                          zero_hz_is_active);
  } else {
    UMA_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.IsZeroHzActive.Window",
                          zero_hz_is_active);
  }
}

void LogDesktopCaptureFrameIsRefresh(DesktopMediaID::Type capturer_type,
                                     bool is_refresh_frame) {
  if (capturer_type == DesktopMediaID::TYPE_SCREEN) {
    UMA_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.FrameIsRefresh.Screen",
                          is_refresh_frame);
  } else {
    UMA_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.FrameIsRefresh.Window",
                          is_refresh_frame);
  }
}

void LogDesktopCaptureFrameRate(DesktopMediaID::Type capturer_type,
                                int frame_rate_fps) {
  if (capturer_type == DesktopMediaID::TYPE_SCREEN) {
    UMA_HISTOGRAM_COUNTS_100("WebRTC.DesktopCapture.FrameRate.Screen",
                             frame_rate_fps);
  } else {
    UMA_HISTOGRAM_COUNTS_100("WebRTC.DesktopCapture.FrameRate.Window",
                             frame_rate_fps);
  }
}

void LogDesktopCaptureRequestRefreshRate(DesktopMediaID::Type capturer_type,
                                         int rrf_rate_fps) {
  if (capturer_type == DesktopMediaID::TYPE_SCREEN) {
    UMA_HISTOGRAM_COUNTS_100("WebRTC.DesktopCapture.RefreshRate.Screen",
                             rrf_rate_fps);
  } else {
    UMA_HISTOGRAM_COUNTS_100("WebRTC.DesktopCapture.RefreshRate.Window",
                             rrf_rate_fps);
  }
}

// Helper class which request that the system-global Windows timer interrupt
// frequency be raised at construction. The corresponding deactivation is done
// at destruction. How high the frequency is raised depends on the system's
// power state and possibly other options. Only supported on Windows.
class ScopedHighResolutionTimer {
 public:
#if !BUILDFLAG(IS_WIN)
  ScopedHighResolutionTimer() {}
#else
  ScopedHighResolutionTimer() {
    if (!base::Time::IsHighResolutionTimerInUse()) {
      enabled_ = base::Time::ActivateHighResolutionTimer(true);
    }
  }
  ~ScopedHighResolutionTimer() {
    if (enabled_) {
      base::Time::ActivateHighResolutionTimer(false);
    }
  }

 private:
  bool enabled_ = false;
#endif
};

}  // namespace

class DesktopCaptureDevice::Core : public webrtc::DesktopCapturer::Callback {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
       std::unique_ptr<webrtc::DesktopCapturer> capturer,
       DesktopMediaID::Type type,
       bool zero_hertz_is_supported);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  // Implementation of VideoCaptureDevice methods.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client);
  // Executes a refresh capture, if conditions permit. Otherwise, schedules a
  // later retry. If a refresh was already pending, a new request is ignored.
  void RequestRefreshFrame();

  base::TimeDelta GetDelayBeforeNextRefreshAttempt() const;

  void SetNotificationWindowId(gfx::NativeViewId window_id);

  void SetMockTimeForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* tick_clock);

  base::WeakPtr<Core> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  // webrtc::DesktopCapturer::Callback interface.
  // A side-effect of this method is to schedule the next frame.
  void OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Method that is scheduled on |task_runner_| to be called on regular interval
  // to capture a frame.
  void OnCaptureTimer();

  // Captures a frame. Upon completion, schedules the next frame. The frame type
  // is a refresh frame if `is_refresh_frame` is true and a default frame
  // otherwise. Sending refresh frames is expected to be a rare event since a
  // refresh request will be canceled by default capture events and they are
  // periodic.
  void CaptureFrame(bool is_refresh_frame);

  // Schedules a timer for the next call to |CaptureFrame|. This method assumes
  // that |CaptureFrame| has already been called at least once before.
  void ScheduleNextCaptureFrame();

  void RequestWakeLock();

  base::TimeTicks NowTicks() const;

  bool zero_hertz_is_supported() const { return zero_hertz_is_supported_; }

  // Requests high-resolution timers on Windows if not already active.
  // Created in AllocateAndStart() and destroyed in ~Core().
  std::unique_ptr<ScopedHighResolutionTimer> scoped_high_res_timer_;

  // Task runner used for capturing operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The underlying DesktopCapturer instance used to capture frames.
  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;

  // The device client which proxies device events to the controller. Accessed
  // on the task_runner_ thread.
  std::unique_ptr<Client> client_;

  // Requested video capture frame rate.
  float requested_frame_rate_;

  // Inverse of the requested frame rate.
  base::TimeDelta requested_frame_duration_;

  // Contains the actual (measured) frame rate using an exponential moving
  // average (EMA) filter. Uses a simple filter with 0.1 weight of the current
  // sample. Unit is in frames per second (fps).
  float frame_rate_;

  // Contains the measured request-refresh rate using an exponential moving
  // average (EMA) filter. Uses a simple filter with 0.1 weight of the current
  // sample. Unit is in frames per second (fps).
  float rrf_rate_;

  // Records time of last call to CaptureFrame.
  base::TimeTicks capture_start_time_;

  // Size of frame most recently captured from the source.
  webrtc::DesktopSize last_frame_size_;

  // DesktopFrame into which captured frames are down-scaled and/or letterboxed,
  // depending upon the caller's requested capture capabilities. If frames can
  // be returned to the caller directly then this is NULL.
  // TODO(https://crbug.com/1444340): should NOT be used to store frames
  // received from the underlying capturer since it can cause cursor flickering
  // if the frame is a DesktopFrameWithCursor. The output frame is black when
  // |output_frame_is_black_| is set. This can happen when a minimized window
  // is shared.
  std::unique_ptr<webrtc::DesktopFrame> output_frame_;

  // True when the |output_frame_->data()| contains only zeros. Tracking this is
  // an optimization to avoid re-clearing |output_frame_| during stretches where
  // we are only sending black frames.
  bool output_frame_is_black_ = false;

  // Determines the size of frames to deliver to the |client_|.
  media::CaptureResolutionChooser resolution_chooser_;

  raw_ptr<const base::TickClock> tick_clock_ = nullptr;

  // Timer used to capture the frame.
  std::unique_ptr<base::OneShotTimer> capture_timer_;

  // See above description of kDefaultMaximumCpuConsumptionPercentage.
  int max_cpu_consumption_percentage_;

  // True when waiting for |desktop_capturer_| to capture current frame.
  bool capture_in_progress_ = false;

  // True when waiting for |desktop_capturer_| to capture current frame as a
  // response to refresh frame request.
  bool refresh_in_progress_ = false;

  // True if the first capture call has returned. Used to log the first capture
  // result.
  bool first_capture_returned_ = false;

  // True if the first capture permanent error has been logged. Used to log the
  // first capture permanent error.
  bool first_permanent_error_logged = false;

  // The type of the capturer.
  DesktopMediaID::Type capturer_type_;

  // True if we support dropping captured frames where the updated region
  // contains no change since last captured frame. To support this 0Hz mode,
  // the utilized capturer implementation must updates the
  // |DesktopFrame::updated_region()| desktop region for each captured frame.
  const bool zero_hertz_is_supported_;

  // The system time when we receive the first frame.
  base::TimeTicks first_ref_time_;

  // The time when Core::CaptureFrame() is called. Used to derive the delta
  // time since last call. The delta time then drives the frame-rate filter
  // which results in an average capture frame rate in `frame_rate_`.
  base::TimeTicks last_capture_time_;

  // The time when Core::RequestRefreshFrame() is called. Used to derive the
  // delta time since last call. The delta time then drives the refresh-rate
  // filter which results in an average refresh rate in `rrf_rate_`.
  base::TimeTicks last_rrf_time_;

  std::unique_ptr<webrtc::BasicDesktopFrame> black_frame_;

  // TODO(jiayl): Remove wake_lock_ when there is an API to keep the
  // screen from sleeping for the drive-by web.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::WeakPtrFactory<Core> weak_factory_{this};
};

DesktopCaptureDevice::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type,
    bool zero_hertz_is_supported)
    : task_runner_(task_runner),
      desktop_capturer_(std::move(capturer)),
      capture_timer_(new base::OneShotTimer()),
      max_cpu_consumption_percentage_(kDefaultMaximumCpuConsumptionPercentage),
      capture_in_progress_(false),
      first_capture_returned_(false),
      first_permanent_error_logged(false),
      capturer_type_(type),
      zero_hertz_is_supported_(zero_hertz_is_supported) {}

DesktopCaptureDevice::Core::~Core() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_.reset();
  output_frame_.reset();
  last_frame_size_.set(0, 0);
  desktop_capturer_.reset();
}

void DesktopCaptureDevice::Core::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GT(params.requested_format.frame_size.GetArea(), 0);
  DCHECK_GT(params.requested_format.frame_rate, 0);
  DCHECK(desktop_capturer_);
  DCHECK(client);
  DCHECK(!client_);

  scoped_high_res_timer_ = std::make_unique<ScopedHighResolutionTimer>();
  client_ = std::move(client);
  requested_frame_rate_ = params.requested_format.frame_rate;
  frame_rate_ = requested_frame_rate_;
  rrf_rate_ = 0;
  requested_frame_duration_ = base::Microseconds(static_cast<int64_t>(
      static_cast<double>(base::Time::kMicrosecondsPerSecond) /
          requested_frame_rate_ +
      0.5 /* round to nearest int */));

  // Pass the min/max resolution and fixed aspect ratio settings from |params|
  // to the CaptureResolutionChooser.
  const auto constraints = params.SuggestConstraints();
  resolution_chooser_.SetConstraints(constraints.min_frame_size,
                                     constraints.max_frame_size,
                                     constraints.fixed_aspect_ratio);
  VLOG(1) << __func__ << " (requested_frame_rate=" << requested_frame_rate_
          << ", max_frame_size=" << constraints.max_frame_size.ToString()
          << ", requested_frame_duration="
          << requested_frame_duration_.InMilliseconds()
          << ", max_cpu_consumption_percentage="
          << max_cpu_consumption_percentage_ << ")";

  DCHECK(!wake_lock_);
  RequestWakeLock();

  desktop_capturer_->Start(this);
  // Assume it will be always started successfully for now.
  client_->OnStarted();

  CaptureFrame(/*is_refresh_frame=*/false);
}

void DesktopCaptureDevice::Core::RequestRefreshFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("webrtc", __func__);
  VLOG(2) << __func__;

  if (!client_) {
    return;
  }

  const base::TimeTicks now = NowTicks();
  if (last_rrf_time_.is_null()) {
    last_rrf_time_ = now;
  } else {
    const base::TimeDelta delta_ms = now - last_rrf_time_;
    // We use an exponential moving average (EMA) filter to calculate the
    // current RRF frame rate (in frames per second).
    const float input_frame_rate_fps = (1000.0 / delta_ms.InMillisecondsF());
    rrf_rate_ = kAlpha * input_frame_rate_fps + (1.0 - kAlpha) * rrf_rate_;
    last_rrf_time_ = now;
    VLOG(2) << " rrf_delta_ms=" << delta_ms.InMillisecondsF()
            << ", rrf_rate=" << frame_rate_ << " [fps]";
    const int rrf_rate_fps = base::saturated_cast<int>(frame_rate_ + 0.5);
    LogDesktopCaptureRequestRefreshRate(capturer_type_, rrf_rate_fps);
  }

  if (!capture_in_progress_) {
    CaptureFrame(/*is_refresh_frame=*/true);
  }
}

void DesktopCaptureDevice::Core::SetNotificationWindowId(
    gfx::NativeViewId window_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(window_id);
  desktop_capturer_->SetExcludedWindow(window_id);
}

void DesktopCaptureDevice::Core::SetMockTimeForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  capture_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  capture_timer_->SetTaskRunner(task_runner);
}

void DesktopCaptureDevice::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(client_);
  DCHECK(capture_in_progress_ || refresh_in_progress_);
  capture_in_progress_ = false;
  const bool frame_is_refresh = refresh_in_progress_;
  refresh_in_progress_ = false;
  TRACE_EVENT1("webrtc", __func__, "frame_is_refresh", frame_is_refresh);

  bool success = result == webrtc::DesktopCapturer::Result::SUCCESS;

  if (!first_capture_returned_) {
    first_capture_returned_ = true;
    if (capturer_type_ == DesktopMediaID::TYPE_SCREEN) {
      IncrementDesktopCaptureCounter(success ? FIRST_SCREEN_CAPTURE_SUCCEEDED
                                             : FIRST_SCREEN_CAPTURE_FAILED);
    } else {
      IncrementDesktopCaptureCounter(success ? FIRST_WINDOW_CAPTURE_SUCCEEDED
                                             : FIRST_WINDOW_CAPTURE_FAILED);
    }
  }

  if (!success) {
    VLOG(2) << __func__ << " [ERROR]";
    if (result == webrtc::DesktopCapturer::Result::ERROR_PERMANENT) {
      if (!first_permanent_error_logged) {
        first_permanent_error_logged = true;
        if (capturer_type_ == DesktopMediaID::TYPE_SCREEN) {
          IncrementDesktopCaptureCounter(SCREEN_CAPTURER_PERMANENT_ERROR);
        } else {
          IncrementDesktopCaptureCounter(WINDOW_CAPTURER_PERMANENT_ERROR);
        }
      }
      client_->OnError(media::VideoCaptureError::
                           kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed,
                       FROM_HERE, "The desktop capturer has failed.");
      return;
    }
    // Continue capturing frames in the temporary error case.
    ScheduleNextCaptureFrame();
    return;
  }
  DCHECK(frame);

  // Continue capturing frames when there are no changes in updated regions
  // since the last captured frame but don't send the same frame again to the
  // client. Checking `first_ref_time_` ensures that at least one frame has been
  // captured before 0Hz can be activated. The zero-hertz mode is disabled if
  // the captured frame is a refresh frame to guarantee that the client actually
  // receives a new frame when explicitly asking for it.
  // |zero_hertz_is_supported()| can be false in combination with capturers that
  // do not support the 0Hz mode, e.g. Windows capturers using the WGC API.
  const bool zero_hertz_is_active =
      zero_hertz_is_supported() && !first_ref_time_.is_null() &&
      !frame_is_refresh && frame->updated_region().is_empty();
  VLOG(2) << __func__ << " [SUCCESS]" << (frame_is_refresh ? "[RRF]" : "")
          << (zero_hertz_is_active ? "[0Hz]" : "");
  if (zero_hertz_is_supported()) {
    LogDesktopCaptureZeroHzIsActive(capturer_type_, zero_hertz_is_active);
  }
  if (zero_hertz_is_active) {
    ScheduleNextCaptureFrame();
    return;
  }

  // If the frame size has changed, drop the output frame (if any), and
  // determine the new output size.
  if (!last_frame_size_.equals(frame->size())) {
    output_frame_.reset();
    resolution_chooser_.SetSourceSize(
        gfx::Size(frame->size().width(), frame->size().height()));
    last_frame_size_ = frame->size();
  }
  // Align to 2x2 pixel boundaries, as required by OnIncomingCapturedData() so
  // it can convert the frame to I420 format.
  webrtc::DesktopSize output_size(
      resolution_chooser_.capture_size().width() & ~1,
      resolution_chooser_.capture_size().height() & ~1);
  if (output_size.is_empty()) {
    // Even RESOLUTION_POLICY_ANY_WITHIN_LIMIT is used, a non-empty size should
    // be guaranteed.
    output_size.set(2, 2);
  }
  VLOG(2) << __func__ << " [output_size=(" << output_size.width() << "x"
          << output_size.height() << ")]";

  size_t output_bytes = output_size.width() * output_size.height() *
                        webrtc::DesktopFrame::kBytesPerPixel;
  const uint8_t* output_data = nullptr;

  if (frame->size().width() <= 1 || frame->size().height() <= 1) {
    // On OSX We receive a 1x1 frame when the shared window is minimized. It
    // cannot be subsampled to I420 and will be dropped downstream. So we
    // replace it with a black frame to avoid the video appearing frozen at the
    // last frame.
    if (!output_frame_ || !output_frame_->size().equals(output_size)) {
      // The new frame will be black by default.
      output_frame_ = std::make_unique<webrtc::BasicDesktopFrame>(output_size);
      output_frame_is_black_ = true;
    }
    if (!output_frame_is_black_) {
      output_frame_->SetFrameDataToBlack();
      output_frame_is_black_ = true;
    }
  } else {
    // Scaling frame with odd dimensions to even dimensions will cause
    // blurring. See https://crbug.com/737278.
    // Since chromium always requests frames to be with even dimensions,
    // i.e. for I420 format and video codec, always cropping captured frame
    // to even dimensions.
    const int32_t frame_width = frame->size().width();
    const int32_t frame_height = frame->size().height();
    // TODO(braveyao): remove the check once |CreateCroppedDesktopFrame| can
    // do this check internally.
    if (frame_width & 1 || frame_height & 1) {
      frame = webrtc::CreateCroppedDesktopFrame(
          std::move(frame),
          webrtc::DesktopRect::MakeWH(frame_width & ~1, frame_height & ~1));
    }
    DCHECK(frame);
    DCHECK(!frame->size().is_empty());

    if (!frame->size().equals(output_size)) {
      VLOG(2) << "  Downscaling: frame->size=(" << frame->size().width() << "x"
              << frame->size().height() << ")";
      // Down-scale and/or letterbox to the target format if the frame does
      // not match the output size.

      // Allocate a buffer of the correct size to scale the frame into.
      // |output_frame_| is cleared whenever the output size changes, so we
      // don't need to worry about clearing out stale pixel data in
      // letterboxed areas.
      if (!output_frame_) {
        output_frame_ =
            std::make_unique<webrtc::BasicDesktopFrame>(output_size);
      }
      DCHECK(output_frame_->size().equals(output_size));

      // TODO(wez): Optimize this to scale only changed portions of the
      // output, using ARGBScaleClip().
      const webrtc::DesktopRect output_rect =
          ComputeLetterboxRect(output_size, frame->size());
      uint8_t* output_rect_data =
          output_frame_->GetFrameDataAtPos(output_rect.top_left());
      libyuv::ARGBScale(frame->data(), frame->stride(), frame->size().width(),
                        frame->size().height(), output_rect_data,
                        output_frame_->stride(), output_rect.width(),
                        output_rect.height(), libyuv::kFilterBilinear);
      output_data = output_frame_->data();
      output_frame_is_black_ = false;
    } else if (IsFrameUnpackedOrInverted(frame.get())) {
      // If |frame| is not packed top-to-bottom then create a packed
      // top-to-bottom copy. This is required if the frame is inverted (see
      // crbug.com/306876), or if |frame| is cropped form a larger frame (see
      // crbug.com/437740).
      if (!output_frame_) {
        output_frame_ =
            std::make_unique<webrtc::BasicDesktopFrame>(output_size);
      }
      output_frame_->CopyPixelsFrom(
          *frame, webrtc::DesktopVector(),
          webrtc::DesktopRect::MakeSize(frame->size()));
      output_data = output_frame_->data();
      output_frame_is_black_ = false;
    } else {
      // If the captured frame matches the output size, we can return the pixel
      // data directly.
      output_data = frame->data();
      output_frame_is_black_ = false;
    }
  }

  gfx::ColorSpace frame_color_space;
  if (!frame->icc_profile().empty()) {
    gfx::ICCProfile icc_profile = gfx::ICCProfile::FromData(
        frame->icc_profile().data(), frame->icc_profile().size());
    frame_color_space = icc_profile.GetColorSpace();
  }

  base::TimeTicks now = NowTicks();
  if (first_ref_time_.is_null())
    first_ref_time_ = now;
  client_->OnIncomingCapturedData(
      output_data, output_bytes,
      media::VideoCaptureFormat(
          gfx::Size(output_size.width(), output_size.height()),
          requested_frame_rate_, media::PIXEL_FORMAT_ARGB),
      frame_color_space, 0 /* clockwise_rotation */, false /* flip_y */, now,
      now - first_ref_time_, std::nullopt);

  ScheduleNextCaptureFrame();
}

void DesktopCaptureDevice::Core::OnCaptureTimer() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!client_)
    return;

  CaptureFrame(/*is_refresh_frame=*/false);
}

void DesktopCaptureDevice::Core::CaptureFrame(bool is_refresh_frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!capture_in_progress_);
  TRACE_EVENT1("webrtc", __func__, "is_refresh_frame", is_refresh_frame);
  VLOG(2) << __func__ << "(is_refresh_frame=" << is_refresh_frame << ")";
  LogDesktopCaptureFrameIsRefresh(capturer_type_, is_refresh_frame);

  capture_start_time_ = NowTicks();

  if (!is_refresh_frame) {
    capture_in_progress_ = true;
  } else {
    refresh_in_progress_ = true;
  }

  // Track the average frame rate for the default capture path. Frame rate for
  // request refresh frames is tracked separately by calls to
  // RequestRefreshFrame (see `rrf_rate_`);
  if (!is_refresh_frame) {
    if (last_capture_time_.is_null()) {
      last_capture_time_ = capture_start_time_;
    } else {
      const base::TimeDelta delta_ms = capture_start_time_ - last_capture_time_;
      // We use an exponential moving average (EMA) filter to calculate the
      // current frame rate (in frames per second). The filter has the following
      // difference (time-domain) equation:
      //   y[i]=α⋅x[i]+(1-α)⋅y[i−1]
      // where
      //   y is the output, [i] denotes the sample number, x is the input, and α
      //   is a constant which sets the cutoff frequency (a value between 0 and
      //   1 where 1 corresponds to "no filtering").
      // A value of α=0.1 results in a suitable amount of smoothing.
      const float input_frame_rate_fps = (1000.0 / delta_ms.InMillisecondsF());
      frame_rate_ =
          kAlpha * input_frame_rate_fps + (1.0 - kAlpha) * frame_rate_;
      last_capture_time_ = capture_start_time_;
      VLOG(2) << " delta_ms=" << delta_ms.InMillisecondsF()
              << ", frame_rate=" << frame_rate_ << " [fps]";
      const int frame_rate_fps = base::saturated_cast<int>(frame_rate_ + 0.5);
      LogDesktopCaptureFrameRate(capturer_type_, frame_rate_fps);
    }
  }

  desktop_capturer_->CaptureFrame();
}

void DesktopCaptureDevice::Core::ScheduleNextCaptureFrame() {
  // Make sure CaptureFrame() was called at least once before.
  DCHECK(!capture_start_time_.is_null());

  base::TimeDelta last_capture_duration = NowTicks() - capture_start_time_;
  VLOG(2) << __func__ << " [last_capture_duration="
          << last_capture_duration.InMilliseconds() << "]";

  // Limit frame-rate to reduce CPU consumption.
  base::TimeDelta capture_period =
      std::max((last_capture_duration * 100) / max_cpu_consumption_percentage_,
               requested_frame_duration_);
  VLOG(2) << "  capture_period=" << capture_period.InMilliseconds();
  VLOG(2) << "  timer(dT="
          << (capture_period - last_capture_duration).InMilliseconds() << ")";

  // Schedule a task for the next frame.
  capture_timer_->Start(FROM_HERE, capture_period - last_capture_duration, this,
                        &Core::OnCaptureTimer);
}

void DesktopCaptureDevice::Core::RequestWakeLock() {
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  auto receiver = wake_lock_provider.BindNewPipeAndPassReceiver();
  // TODO(crbug.com/41377723): Fix DesktopCaptureDeviceTest and remove
  // this conditional.
  if (BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BindWakeLockProvider, std::move(receiver)));
  }

  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "Native desktop capture",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

base::TimeTicks DesktopCaptureDevice::Core::NowTicks() const {
  return tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
}

// static
std::unique_ptr<media::VideoCaptureDevice> DesktopCaptureDevice::Create(
    const DesktopMediaID& source) {
  VLOG(1) << __func__ << "(source=" << source.ToString() << ")";
  auto options = desktop_capture::CreateDesktopCaptureOptions();
  std::unique_ptr<webrtc::DesktopCapturer> capturer;
  std::unique_ptr<media::VideoCaptureDevice> result;

#if BUILDFLAG(IS_WIN)
  options.set_allow_cropping_window_capturer(true);

  // We prefer to allow the WGC and DXGI capturers to embed the cursor when
  // possible. The DXGI implementation uses this switch in combination with
  // internal checks for support of if it is possible to embed the cursor.
  // Note that, very few graphical adapters support embedding the cursor into
  // the captured frame in combination with DXGI; hence most cursors will be
  // added separately by a desktop and cursor composer even if this option is
  // set to true. GDI does not use this option.
  // TODO(crbug.com/40259358): Possibly remove this flag. Keeping for now to
  // force non embedded cursor for all capture APIs on Windows.
  static BASE_FEATURE(kAllowWinCursorEmbedded, "AllowWinCursorEmbedded",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  if (base::FeatureList::IsEnabled(kAllowWinCursorEmbedded)) {
    options.set_prefer_cursor_embedded(true);
  }
  if (base::FeatureList::IsEnabled(features::kWebRtcAllowWgcScreenCapturer)) {
    options.set_allow_wgc_screen_capturer(true);

    // 0Hz support is by default disabled for WGC but it can be enabled using
    // the `kWebRtcAllowWgcZeroHz` feature flag. When enabled, the WGC capturer
    // will compare the pixel values of the new frame and the previous frame and
    // update the DesktopRegion part of the frame to reflect if the content has
    // changed or not. DesktopFrame::updated_region() will be empty if nothing
    // has changed and contain one (damage) region corresponding to the complete
    // screen or window being captured if any change is detected.
    if (source.type == DesktopMediaID::TYPE_SCREEN) {
      options.set_allow_wgc_zero_hertz(
          base::FeatureList::IsEnabled(features::kWebRtcAllowWgcScreenZeroHz));
    }
  }
  if (base::FeatureList::IsEnabled(features::kWebRtcAllowWgcWindowCapturer)) {
    options.set_allow_wgc_window_capturer(true);
    if (source.type == DesktopMediaID::TYPE_WINDOW) {
      options.set_allow_wgc_zero_hertz(
          base::FeatureList::IsEnabled(features::kWebRtcAllowWgcWindowZeroHz));
    }
  }
  VLOG(1) << "DesktopCaptureOptions: options={prefer_cursor_embedded: "
          << options.prefer_cursor_embedded() << ", allow_wgc_screen_capturer: "
          << options.allow_wgc_screen_capturer()
          << ", allow_wgc_window_capturer: "
          << options.allow_wgc_window_capturer()
          << ", allow_wgc_zero_hertz: " << options.allow_wgc_zero_hertz()
          << "}";
#endif

  // For browser tests, to create a fake desktop capturer.
  if (source.id == DesktopMediaID::kFakeId) {
    capturer = std::make_unique<webrtc::FakeDesktopCapturer>();
    result.reset(new DesktopCaptureDevice(std::move(capturer), source.type));
    return result;
  }

  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN: {
      std::unique_ptr<webrtc::DesktopCapturer> screen_capturer(
          webrtc::DesktopCapturer::CreateScreenCapturer(options));
      if (screen_capturer && screen_capturer->SelectSource(source.id)) {
        capturer = std::make_unique<webrtc::DesktopAndCursorComposer>(
            std::move(screen_capturer), options);
        IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
        IncrementDesktopCaptureCounter(
            source.audio_share ? SCREEN_CAPTURER_CREATED_WITH_AUDIO
                               : SCREEN_CAPTURER_CREATED_WITHOUT_AUDIO);
      }
      break;
    }

    case DesktopMediaID::TYPE_WINDOW: {
      std::unique_ptr<webrtc::DesktopCapturer> window_capturer =
          webrtc::DesktopCapturer::CreateWindowCapturer(options);
      if (window_capturer && window_capturer->SelectSource(source.id)) {
        capturer = std::make_unique<webrtc::DesktopAndCursorComposer>(
            std::move(window_capturer), options);
        IncrementDesktopCaptureCounter(WINDOW_CAPTURER_CREATED);
      }
      break;
    }

    default: {
      NOTREACHED_IN_MIGRATION();
    }
  }

  if (capturer)
    result.reset(new DesktopCaptureDevice(std::move(capturer), source.type));

  return result;
}

DesktopCaptureDevice::~DesktopCaptureDevice() {
  DCHECK(!core_);
}

void DesktopCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::AllocateAndStart, core_->GetWeakPtr(),
                                params, std::move(client)));
}

void DesktopCaptureDevice::StopAndDeAllocate() {
  if (core_) {
    // This thread should mostly be an idle observer. Stopping it should be
    // fast.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
    thread_.task_runner()->DeleteSoon(FROM_HERE, core_.release());
    thread_.Stop();
  }
}

void DesktopCaptureDevice::RequestRefreshFrame() {
  // Refresh request shall have no effect after the capturer has been stopped.
  if (!core_) {
    return;
  }
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::RequestRefreshFrame, core_->GetWeakPtr()));
}

void DesktopCaptureDevice::SetNotificationWindowId(
    gfx::NativeViewId window_id) {
  // This may be called after the capturer has been stopped.
  if (!core_)
    return;
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetNotificationWindowId,
                                core_->GetWeakPtr(), window_id));
}

DesktopCaptureDevice::DesktopCaptureDevice(
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type)
    : thread_("desktopCaptureThread") {
  DVLOG(1) << __func__ << "(type=" << DesktopMediaTypeToString(type) << ")";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // On Windows/OSX the thread must be a UI thread.
  base::MessagePumpType thread_type = base::MessagePumpType::UI;
#else
  base::MessagePumpType thread_type = base::MessagePumpType::DEFAULT;
#endif
  bool zero_hertz_is_supported = true;
#if BUILDFLAG(IS_WIN)
  const bool wgc_screen_zero_hertz =
      base::FeatureList::IsEnabled(features::kWebRtcAllowWgcScreenZeroHz);
  const bool wgc_window_zero_hertz =
      base::FeatureList::IsEnabled(features::kWebRtcAllowWgcWindowZeroHz);
  // TODO(crbug.com/40259358): 0Hz mode seems to cause a flickering
  // cursor in some setups. This flag allows us to disable 0Hz when needed.
  const bool dxgi_gdi_zero_hertz =
      base::FeatureList::IsEnabled(features::kWebRtcAllowDxgiGdiZeroHz);
  const bool wgc_screen_capturer =
      base::FeatureList::IsEnabled(features::kWebRtcAllowWgcScreenCapturer);
  const bool wgc_window_capturer =
      base::FeatureList::IsEnabled(features::kWebRtcAllowWgcWindowCapturer);
  if (!wgc_window_capturer && !wgc_screen_capturer) {
    zero_hertz_is_supported = dxgi_gdi_zero_hertz;
  } else if (!wgc_window_capturer && wgc_screen_capturer) {
    zero_hertz_is_supported = (type == DesktopMediaID::TYPE_SCREEN)
                                  ? wgc_screen_zero_hertz
                                  : dxgi_gdi_zero_hertz;
  } else if (wgc_window_capturer && !wgc_screen_capturer) {
    zero_hertz_is_supported = (type == DesktopMediaID::TYPE_WINDOW)
                                  ? wgc_window_zero_hertz
                                  : dxgi_gdi_zero_hertz;
  } else {
    if (type == DesktopMediaID::TYPE_SCREEN) {
      zero_hertz_is_supported = wgc_screen_zero_hertz;
    } else if (type == DesktopMediaID::TYPE_WINDOW) {
      zero_hertz_is_supported = wgc_window_zero_hertz;
    } else {
      zero_hertz_is_supported = false;
    }
  }
  VLOG(1) << __func__ << " [zero_hertz_is_supported=" << zero_hertz_is_supported
          << "]";
#endif

  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  core_ = std::make_unique<Core>(thread_.task_runner(), std::move(capturer),
                                 type, zero_hertz_is_supported);
}

void DesktopCaptureDevice::SetMockTimeForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  core_->SetMockTimeForTesting(task_runner, tick_clock);  // IN-TEST
}

}  // namespace content
