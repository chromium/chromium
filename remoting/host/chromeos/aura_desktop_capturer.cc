// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "remoting/host/chromeos/ash_display_util.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"

namespace remoting {

namespace {

std::unique_ptr<webrtc::DesktopFrame> ToDesktopFrame(
    int dpi,
    absl::optional<SkBitmap> bitmap) {
  if (!bitmap)
    return nullptr;

  std::unique_ptr<webrtc::DesktopFrame> frame(SkiaBitmapDesktopFrame::Create(
      std::make_unique<SkBitmap>(std::move(bitmap.value()))));

  frame->set_dpi(webrtc::DesktopVector(dpi, dpi));

  // |VideoFramePump| will not encode the frame if |updated_region| is empty.
  const webrtc::DesktopRect& rect = webrtc::DesktopRect::MakeWH(
      frame->size().width(), frame->size().height());
  frame->mutable_updated_region()->SetRect(rect);

  return frame;
}

}  // namespace

AuraDesktopCapturer::AuraDesktopCapturer()
    : AuraDesktopCapturer(AshDisplayUtil::Get()) {}

AuraDesktopCapturer::AuraDesktopCapturer(AshDisplayUtil& util) : util_(util) {}

AuraDesktopCapturer::~AuraDesktopCapturer() = default;

void AuraDesktopCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
  DCHECK(!callback_) << "Start() can only be called once";
  callback_ = callback;
  DCHECK(callback_);

  source_display_id_ = util_.GetPrimaryDisplayId();
}

void AuraDesktopCapturer::CaptureFrame() {
  DCHECK(callback_) << "Call Start() first";

  const display::Display* source = GetSourceDisplay();
  if (!source) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }

  util_.TakeScreenshotOfDisplay(
      source_display_id_,
      base::BindOnce(ToDesktopFrame, util_.GetDpi(*source))
          .Then(base::BindOnce(&AuraDesktopCapturer::OnFrameCaptured,
                               weak_factory_.GetWeakPtr())));
}

void AuraDesktopCapturer::OnFrameCaptured(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (!frame) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }

  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

bool AuraDesktopCapturer::GetSourceList(SourceList* sources) {
  // TODO(zijiehe): Implement screen enumeration.
  sources->push_back({0});
  return true;
}

bool AuraDesktopCapturer::SelectSource(SourceId id) {
  if (!base::FeatureList::IsEnabled(kEnableMultiMonitorsInCrd))
    return false;

  if (!util_.GetDisplayForId(id))
    return false;

  source_display_id_ = id;
  return true;
}

const display::Display* AuraDesktopCapturer::GetSourceDisplay() const {
  return util_.GetDisplayForId(source_display_id_);
}

}  // namespace remoting
