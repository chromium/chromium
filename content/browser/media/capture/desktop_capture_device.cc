// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_util.h"
#include "media/capture/content/capture_resolution_chooser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/webrtc/modules/desktop_capture/cropped_desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/cropping_window_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
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

std::unique_ptr<service_manager::Connector> GetServiceConnector() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(GetSystemConnector());
  return GetSystemConnector()->Clone();
}

int GetMaximumCpuConsumptionPercentage() {
  int max_cpu_consumption_percentage = kDefaultMaximumCpuConsumptionPercentage;

  std::string string_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebRtcMaxCpuConsumptionPercentage);
  int tmp_percentage = 0;
  if (base::StringToInt(string_value, &tmp_percentage)) {
    // If the max cpu percentage provided by the user is outside [1, 100] then
    // |max_cpu_consumption_percentage_| is left to the default value. Same if
    // no value is provided by the user, i.e. |string_value| will be empty and
    // base::StringToInt will set |tmp_percentage| to 0.
    if (tmp_percentage > 0 && tmp_percentage <= 100)
      max_cpu_consumption_percentage = tmp_percentage;
  }

  return max_cpu_consumption_percentage;
}

}  // namespace

class DesktopCaptureDevice::Core : public webrtc::DesktopCapturer::Callback {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
       std::unique_ptr<webrtc::DesktopCapturer> capturer,
       DesktopMediaID::Type type);
  ~Core() override;

  // Implementation of VideoCaptureDevice methods.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client);

  void SetNotificationWindowId(gfx::NativeViewId window_id);

  void SetMockTimeForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* tick_clock);

 private:
  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Method that is scheduled on |task_runner_| to be called on regular interval
  // to capture a frame.
  void OnCaptureTimer();

  // Captures a frame and schedules timer for the next one.
  void CaptureFrameAndScheduleNext();

  // Captures a single frame.
  void DoCapture();

  void RequestWakeLock(std::unique_ptr<service_manager::Connector> connector);

  base::TimeTicks NowTicks() const;

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

  // Size of frame most recently captured from the source.
  webrtc::DesktopSize previous_frame_size_;

  // Determines the size of frames to deliver to the |client_|.
  media::CaptureResolutionChooser resolution_chooser_;

  // DesktopFrame into which captured frames are down-scaled and/or letterboxed,
  // depending upon the caller's requested capture capabilities. If frames can
  // be returned to the caller directly then this is NULL.
  std::unique_ptr<webrtc::DesktopFrame> output_frame_;

  const base::TickClock* tick_clock_ = nullptr;

  // Timer used to capture the frame.
  std::unique_ptr<base::OneShotTimer> capture_timer_;

  // See above description of kDefaultMaximumCpuConsumptionPercentage.
  int max_cpu_consumption_percentage_;

  // True when waiting for |desktop_capturer_| to capture current frame.
  bool capture_in_progress_;

  // True if the first capture call has returned. Used to log the first capture
  // result.
  bool first_capture_returned_;

  // True if the first capture permanent error has been logged. Used to log the
  // first capture permanent error.
  bool first_permanent_error_logged;

  // The type of the capturer.
  DesktopMediaID::Type capturer_type_;

  // The system time when we receive the first frame.
  base::TimeTicks first_ref_time_;

  std::unique_ptr<webrtc::BasicDesktopFrame> black_frame_;

  // TODO(jiayl): Remove wake_lock_ when there is an API to keep the
  // screen from sleeping for the drive-by web.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::WeakPtrFactory<Core> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

DesktopCaptureDevice::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type)
    : task_runner_(task_runner),
      desktop_capturer_(std::move(capturer)),
      capture_timer_(new base::OneShotTimer()),
      max_cpu_consumption_percentage_(GetMaximumCpuConsumptionPercentage()),
      capture_in_progress_(false),
      first_capture_returned_(false),
      first_permanent_error_logged(false),
      capturer_type_(type) {}

DesktopCaptureDevice::Core::~Core() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_.reset();
  output_frame_.reset();
  previous_frame_size_.set(0, 0);
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

  client_ = std::move(client);
  requested_frame_rate_ = params.requested_format.frame_rate;
  requested_frame_duration_ =
      base::TimeDelta::FromMicroseconds(static_cast<int64_t>(
          static_cast<double>(base::Time::kMicrosecondsPerSecond) /
              requested_frame_rate_ +
          0.5 /* round to nearest int */));

  // Pass the min/max resolution and fixed aspect ratio settings from |params|
  // to the CaptureResolutionChooser.
  const auto constraints = params.SuggestConstraints();
  resolution_chooser_.SetConstraints(constraints.min_frame_size,
                                     constraints.max_frame_size,
                                     constraints.fixed_aspect_ratio);

  DCHECK(!wake_lock_);
  // Gets a service_manager::Connector first, then request a wake lock.
  // TODO(https://crbug.com/823869): Fix DesktopCaptureDeviceTest and remove
  // this conditional.
  if (BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI}, base::BindOnce(&GetServiceConnector),
        base::BindOnce(&DesktopCaptureDevice::Core::RequestWakeLock,
                       weak_factory_.GetWeakPtr()));
  }

  desktop_capturer_->Start(this);
  // Assume it will be always started successfully for now.
  client_->OnStarted();

  CaptureFrameAndScheduleNext();
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
  capture_timer_.reset(new base::OneShotTimer(tick_clock_));
  capture_timer_->SetTaskRunner(task_runner);
}

void DesktopCaptureDevice::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(client_);
  DCHECK(capture_in_progress_);
  capture_in_progress_ = false;

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
    }
    return;
  }
  DCHECK(frame);

  base::TimeDelta capture_time(
      base::TimeDelta::FromMilliseconds(frame->capture_time_ms()));

  // The two UMA_ blocks must be put in its own scope since it creates a static
  // variable which expected constant histogram name.
  if (capturer_type_ == DesktopMediaID::TYPE_SCREEN) {
    UMA_HISTOGRAM_TIMES(kUmaScreenCaptureTime, capture_time);
  } else {
    UMA_HISTOGRAM_TIMES(kUmaWindowCaptureTime, capture_time);
  }

  // If the frame size has changed, drop the output frame (if any), and
  // determine the new output size.
  if (!previous_frame_size_.equals(frame->size())) {
    output_frame_.reset();
    resolution_chooser_.SetSourceSize(
        gfx::Size(frame->size().width(), frame->size().height()));
    previous_frame_size_ = frame->size();
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

  size_t output_bytes = output_size.width() * output_size.height() *
      webrtc::DesktopFrame::kBytesPerPixel;
  const uint8_t* output_data = nullptr;

  if (frame->size().width() <= 1 || frame->size().height() <= 1) {
    // On OSX We receive a 1x1 frame when the shared window is minimized. It
    // cannot be subsampled to I420 and will be dropped downstream. So we
    // replace it with a black frame to avoid the video appearing frozen at the
    // last frame.
    if (!black_frame_ || !black_frame_->size().equals(output_size)) {
      black_frame_.reset(new webrtc::BasicDesktopFrame(output_size));
    }
    output_data = black_frame_->data();
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
      // Down-scale and/or letterbox to the target format if the frame does
      // not match the output size.

      // Allocate a buffer of the correct size to scale the frame into.
      // |output_frame_| is cleared whenever the output size changes, so we
      // don't need to worry about clearing out stale pixel data in
      // letterboxed areas.
      if (!output_frame_) {
        output_frame_.reset(new webrtc::BasicDesktopFrame(output_size));
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
    } else if (IsFrameUnpackedOrInverted(frame.get())) {
      // If |frame| is not packed top-to-bottom then create a packed
      // top-to-bottom copy. This is required if the frame is inverted (see
      // crbug.com/306876), or if |frame| is cropped form a larger frame (see
      // crbug.com/437740).
      if (!output_frame_) {
        output_frame_.reset(new webrtc::BasicDesktopFrame(output_size));
      }

      output_frame_->CopyPixelsFrom(
          *frame, webrtc::DesktopVector(),
          webrtc::DesktopRect::MakeSize(frame->size()));
      output_data = output_frame_->data();
    } else {
      // If the captured frame matches the output size, we can return the pixel
      // data directly.
      output_data = frame->data();
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
      now - first_ref_time_);
}

void DesktopCaptureDevice::Core::OnCaptureTimer() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!client_)
    return;

  CaptureFrameAndScheduleNext();
}

void DesktopCaptureDevice::Core::CaptureFrameAndScheduleNext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::TimeTicks started_time = NowTicks();
  DoCapture();
  base::TimeDelta last_capture_duration = NowTicks() - started_time;

  // Limit frame-rate to reduce CPU consumption.
  base::TimeDelta capture_period =
      std::max((last_capture_duration * 100) / max_cpu_consumption_percentage_,
               requested_frame_duration_);

  // Schedule a task for the next frame.
  capture_timer_->Start(FROM_HERE, capture_period - last_capture_duration, this,
                        &Core::OnCaptureTimer);
}

void DesktopCaptureDevice::Core::DoCapture() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!capture_in_progress_);

  capture_in_progress_ = true;
  desktop_capturer_->CaptureFrame();

  // Currently only synchronous implementations of DesktopCapturer are
  // supported.
  DCHECK(!capture_in_progress_);
}

void DesktopCaptureDevice::Core::RequestWakeLock(
    std::unique_ptr<service_manager::Connector> connector) {
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  connector->Connect(device::mojom::kServiceName,
                     wake_lock_provider.BindNewPipeAndPassReceiver());
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
  auto options = desktop_capture::CreateDesktopCaptureOptions();
  std::unique_ptr<webrtc::DesktopCapturer> capturer;
  std::unique_ptr<media::VideoCaptureDevice> result;

  // For browser tests, to create a fake desktop capturer.
  if (source.id == DesktopMediaID::kFakeId) {
    capturer.reset(new webrtc::FakeDesktopCapturer());
    result.reset(new DesktopCaptureDevice(std::move(capturer), source.type));
    return result;
  }

  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN: {
      std::unique_ptr<webrtc::DesktopCapturer> screen_capturer(
          webrtc::DesktopCapturer::CreateScreenCapturer(options));
      if (screen_capturer && screen_capturer->SelectSource(source.id)) {
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            std::move(screen_capturer), options));
        IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
        IncrementDesktopCaptureCounter(
            source.audio_share ? SCREEN_CAPTURER_CREATED_WITH_AUDIO
                               : SCREEN_CAPTURER_CREATED_WITHOUT_AUDIO);
      }
      break;
    }

    case DesktopMediaID::TYPE_WINDOW: {
      std::unique_ptr<webrtc::DesktopCapturer> window_capturer =
          webrtc::CroppingWindowCapturer::CreateCapturer(options);
      if (window_capturer && window_capturer->SelectSource(source.id)) {
        window_capturer->FocusOnSelectedSource();
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            std::move(window_capturer), options));
        IncrementDesktopCaptureCounter(WINDOW_CAPTURER_CREATED);
      }
      break;
    }

    default: { NOTREACHED(); }
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
      FROM_HERE,
      base::BindOnce(&Core::AllocateAndStart, base::Unretained(core_.get()),
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

void DesktopCaptureDevice::SetNotificationWindowId(
    gfx::NativeViewId window_id) {
  // This may be called after the capturer has been stopped.
  if (!core_)
    return;
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetNotificationWindowId,
                                base::Unretained(core_.get()), window_id));
}

DesktopCaptureDevice::DesktopCaptureDevice(
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type)
    : thread_("desktopCaptureThread") {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // On Windows/OSX the thread must be a UI thread.
  base::MessagePumpType thread_type = base::MessagePumpType::UI;
#else
  base::MessagePumpType thread_type = base::MessagePumpType::DEFAULT;
#endif

  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  core_.reset(new Core(thread_.task_runner(), std::move(capturer), type));
}

void DesktopCaptureDevice::SetMockTimeForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  core_->SetMockTimeForTesting(task_runner, tick_clock);
}

}  // namespace content
