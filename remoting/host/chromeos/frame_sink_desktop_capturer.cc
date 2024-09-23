// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/frame_sink_desktop_capturer.h"

#include "base/time/time.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/display/display.h"

namespace remoting {

namespace {
constexpr int kMaxFrameRate = 60;
constexpr auto kPixelFormat = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
constexpr bool kAutoThrottle = false;

bool IsEqual(gfx::Size lhs, webrtc::DesktopSize rhs) {
  return (lhs.width() == rhs.width()) && (lhs.height() == rhs.height());
}

}  // namespace

FrameSinkDesktopCapturer::FrameSinkDesktopCapturer()
    : FrameSinkDesktopCapturer(AshProxy::Get()) {}

FrameSinkDesktopCapturer::FrameSinkDesktopCapturer(AshProxy& ash_proxy)
    : ash_(ash_proxy) {
  LOG(INFO) << "CRD: Starting frame sink desktop capturer";
}

FrameSinkDesktopCapturer::~FrameSinkDesktopCapturer() {
  if (video_capturer_) {
    video_capturer_->Stop();
  }
}

void FrameSinkDesktopCapturer::Start(DesktopCapturer::Callback* callback) {
  DCHECK(!callback_) << "Start() can only be called once";
  DCHECK(callback);
  callback_ = callback;

  video_capturer_.emplace(base::BindRepeating(
      &FrameSinkDesktopCapturer::BindRemote, base::Unretained(this)));

  video_capturer_->SetFormat(kPixelFormat);
  video_capturer_->SetMinCapturePeriod(base::Hertz(kMaxFrameRate));
  // Allow changing of resolution at any time, otherwise the capturer would not
  // send us the real resolution of the display if we switch source display
  // multiple times within the `min size change period`.
  video_capturer_->SetMinSizeChangePeriod(base::Seconds(0));
  // Disable auto-throttling so the capturer will always use the real resolution
  // of the display we're capturing.
  video_capturer_->SetAutoThrottlingEnabled(kAutoThrottle);
  if (source_display_id_ == display::kInvalidDisplayId) {
    source_display_id_ = ash_->GetPrimaryDisplayId();
  }
  SelectSource(source_display_id_);
  video_capturer_->Start(&video_consumer_,
                         viz::mojom::BufferFormatPreference::kDefault);
}

void FrameSinkDesktopCapturer::BindRemote(
    mojo::PendingReceiver<FrameSinkVideoCapturer> pending_receiver) {
  DCHECK(callback_) << "BindRemote must be called after Start()";

  ash_->CreateVideoCapturer(std::move(pending_receiver));
}

void FrameSinkDesktopCapturer::CaptureFrame() {
  const display::Display* source = ash_->GetDisplayForId(source_display_id_);
  if (!source) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame =
      video_consumer_.GetLatestFrame(source->bounds().origin());

  if (!frame) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }
  if (!IsEqual(source->GetSizeInPixel(), frame->size())) {
    SelectSource(source_display_id_);
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool FrameSinkDesktopCapturer::GetSourceList(SourceList* sources) {
  NOTREACHED();
}

bool FrameSinkDesktopCapturer::SelectSource(SourceId id) {
  if (!ash_->GetDisplayForId(id)) {
    return false;
  }

  source_display_id_ = id;
  if (!video_capturer_) {
    // SelectSource() will be called again by Start() after creating the
    // capturer.
    return true;
  }

  scoped_window_capture_request_ =
      ash_->MakeDisplayCapturable(source_display_id_);

  video_capturer_->SetResolutionConstraints(
      GetSourceDisplay()->GetSizeInPixel(),
      GetSourceDisplay()->GetSizeInPixel(),
      /*use_fixed_aspect_ratio=*/false);
  video_capturer_->ChangeTarget(
      viz::VideoCaptureTarget(ash_->GetFrameSinkId(source_display_id_),
                              scoped_window_capture_request_.GetCaptureId()),
      /*sub_capture_target_version=*/0);
  return true;
}

const display::Display* FrameSinkDesktopCapturer::GetSourceDisplay() {
  return ash_->GetDisplayForId(source_display_id_);
}

}  // namespace remoting
