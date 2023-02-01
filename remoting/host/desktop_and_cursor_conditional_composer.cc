// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_and_cursor_conditional_composer.h"

#include "base/functional/bind.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/linux/wayland_utils.h"
#endif

namespace remoting {

DesktopAndCursorConditionalComposer::DesktopAndCursorConditionalComposer(
    std::unique_ptr<DesktopCapturer> desktop_capturer) {
#if defined(WEBRTC_USE_GIO)
  desktop_capturer_ = desktop_capturer.get();
#endif
  capturer_ = webrtc::DesktopAndCursorComposer::CreateWithoutMouseCursorMonitor(
      std::move(desktop_capturer));
}

DesktopAndCursorConditionalComposer::~DesktopAndCursorConditionalComposer() =
    default;

void DesktopAndCursorConditionalComposer::SetComposeEnabled(bool enabled) {
  if (enabled == compose_enabled_) {
    return;
  }

  if (enabled) {
    if (mouse_cursor_) {
      capturer_->OnMouseCursor(webrtc::MouseCursor::CopyOf(*mouse_cursor_));
    }
  } else {
    webrtc::MouseCursor* empty = new webrtc::MouseCursor(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(0, 0)),
        webrtc::DesktopVector(0, 0));
    capturer_->OnMouseCursor(empty);
  }

  compose_enabled_ = enabled;
}

void DesktopAndCursorConditionalComposer::SetMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  mouse_cursor_ = std::move(mouse_cursor);
  if (compose_enabled_) {
    capturer_->OnMouseCursor(webrtc::MouseCursor::CopyOf(*mouse_cursor_));
  }
}

void DesktopAndCursorConditionalComposer::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  if (compose_enabled_) {
    capturer_->OnMouseCursorPosition(position);
  }
}

void DesktopAndCursorConditionalComposer::Start(
    webrtc::DesktopCapturer::Callback* callback) {
  capturer_->Start(callback);
}

void DesktopAndCursorConditionalComposer::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void DesktopAndCursorConditionalComposer::CaptureFrame() {
  capturer_->CaptureFrame();
}

void DesktopAndCursorConditionalComposer::SetExcludedWindow(
    webrtc::WindowId window) {
  capturer_->SetExcludedWindow(window);
}

bool DesktopAndCursorConditionalComposer::GetSourceList(SourceList* sources) {
  return capturer_->GetSourceList(sources);
}

bool DesktopAndCursorConditionalComposer::SelectSource(SourceId id) {
  return capturer_->SelectSource(id);
}

bool DesktopAndCursorConditionalComposer::FocusOnSelectedSource() {
  return capturer_->FocusOnSelectedSource();
}

bool DesktopAndCursorConditionalComposer::IsOccluded(
    const webrtc::DesktopVector& pos) {
  return capturer_->IsOccluded(pos);
}

bool DesktopAndCursorConditionalComposer::SupportsFrameCallbacks() {
#if BUILDFLAG(IS_LINUX)
  return IsRunningWayland();
#else
  return false;
#endif
}

void DesktopAndCursorConditionalComposer::SetMaxFrameRate(
    uint32_t max_frame_rate) {
  capturer_->SetMaxFrameRate(max_frame_rate);
}

#if defined(WEBRTC_USE_GIO)
void DesktopAndCursorConditionalComposer::GetMetadataAsync(
    base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback) {
  desktop_capturer_->GetMetadataAsync(std::move(callback));
}
#endif

}  // namespace remoting
