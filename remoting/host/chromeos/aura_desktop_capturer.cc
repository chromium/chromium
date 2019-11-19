// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include <utility>

#include "base/bind.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif

namespace remoting {

AuraDesktopCapturer::AuraDesktopCapturer()
    : callback_(nullptr), desktop_window_(nullptr) {}

AuraDesktopCapturer::~AuraDesktopCapturer() = default;

void AuraDesktopCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
#if defined(OS_CHROMEOS)
  if (ash::Shell::HasInstance()) {
    // TODO(kelvinp): Use ash::Shell::GetAllRootWindows() when multiple monitor
    // support is implemented.
    desktop_window_ = ash::Shell::GetPrimaryRootWindow();
    DCHECK(desktop_window_) << "Failed to retrieve the Aura Shell root window";
  }
#endif

  DCHECK(!callback_) << "Start() can only be called once";
  callback_ = callback;
  DCHECK(callback_);
}

void AuraDesktopCapturer::CaptureFrame() {
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&AuraDesktopCapturer::OnFrameCaptured,
                         weak_factory_.GetWeakPtr()));

  gfx::Rect window_rect(desktop_window_->bounds().size());

  request->set_area(window_rect);
  desktop_window_->layer()->RequestCopyOfOutput(std::move(request));
}

void AuraDesktopCapturer::OnFrameCaptured(
    std::unique_ptr<viz::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame(SkiaBitmapDesktopFrame::Create(
      std::make_unique<SkBitmap>(result->AsSkBitmap())));

  // |VideoFramePump| will not encode the frame if |updated_region| is empty.
  const webrtc::DesktopRect& rect = webrtc::DesktopRect::MakeWH(
      frame->size().width(), frame->size().height());

  // TODO(kelvinp): Set Frame DPI according to the screen resolution.
  // See cc::Layer::contents_scale_(x|y)() and frame->set_depi().
  frame->mutable_updated_region()->SetRect(rect);

  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

bool AuraDesktopCapturer::GetSourceList(SourceList* sources) {
  // TODO(zijiehe): Implement screen enumeration.
  sources->push_back({0});
  return true;
}

bool AuraDesktopCapturer::SelectSource(SourceId id) {
  // TODO(zijiehe): Implement screen selection.
  return true;
}

}  // namespace remoting
