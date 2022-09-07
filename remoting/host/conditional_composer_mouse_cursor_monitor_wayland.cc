// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/conditional_composer_mouse_cursor_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "remoting/base/logging.h"
#include "remoting/host/mouse_shape_pump.h"

namespace remoting {

namespace {

// Wrapper class for handling the mouse cursor monitor callbacks.
class MouseCursorMonitorCallback : public webrtc::MouseCursorMonitor::Callback {
 public:
  explicit MouseCursorMonitorCallback(
      base::WeakPtr<DesktopAndCursorConditionalComposer> composer);

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

 private:
  base::WeakPtr<DesktopAndCursorConditionalComposer> composer_;
};

MouseCursorMonitorCallback::MouseCursorMonitorCallback(
    base::WeakPtr<DesktopAndCursorConditionalComposer> composer)
    : composer_(composer) {}

void MouseCursorMonitorCallback::OnMouseCursor(
    webrtc::MouseCursor* mouse_cursor) {
  // This method should take ownership of |mouse_cursor|.
  std::unique_ptr<webrtc::MouseCursor> owned_cursor(mouse_cursor);
  if (composer_) {
    composer_->SetMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(*owned_cursor)));
  }
}

void MouseCursorMonitorCallback::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  if (composer_)
    composer_->SetMouseCursorPosition(position);
}

// This is the Wayland implementation of the manager class which allows creation
// of pairs of composers and mouse shape pump monitors.
class ComposingCapturerCursorMonitorManagerWayland
    : public ComposingCapturerCursorMonitorManager {
 public:
  explicit ComposingCapturerCursorMonitorManagerWayland(
      DesktopEnvironment* desktop_environment);

  // ComposingCapturerCursorMonitorManager implementation.
  std::unique_ptr<DesktopAndCursorConditionalComposer>
  CreateComposingVideoCapturer(protocol::ClientStub* client_stub) override;

 private:
  base::raw_ptr<DesktopEnvironment> desktop_environment_;

  std::vector<std::unique_ptr<MouseCursorMonitorCallback>>
      mouse_cursor_monitor_callbacks_;
  std::vector<std::unique_ptr<MouseShapePump>> mouse_shape_pumps_;
};

ComposingCapturerCursorMonitorManagerWayland::
    ComposingCapturerCursorMonitorManagerWayland(
        DesktopEnvironment* desktop_environment)
    : desktop_environment_(desktop_environment) {}

std::unique_ptr<DesktopAndCursorConditionalComposer>
ComposingCapturerCursorMonitorManagerWayland::CreateComposingVideoCapturer(
    protocol::ClientStub* client_stub) {
  // Create a desktop capture option that is shared between the composer and
  // mouse cursor monitor. Note that each default capture options has a
  // screencast stream embedded in it and each such stream is unique in the
  // sense that is capturing from a different source and hence can't be
  // shared with other capturers and / or mouse cursor monitors.
  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();
  options.set_allow_pipewire(true);

  auto composer = std::make_unique<DesktopAndCursorConditionalComposer>(
      desktop_environment_->CreateVideoCapturer(options));
  DCHECK(composer);
  auto& callback = mouse_cursor_monitor_callbacks_.emplace_back(
      std::make_unique<MouseCursorMonitorCallback>(composer->GetWeakPtr()));

  // TODO(salmanmalik): Ensure that the mouse shape pump stops as soon as the
  // corresponding video stream is deleted.
  auto& pump = mouse_shape_pumps_.emplace_back(std::make_unique<MouseShapePump>(
      desktop_environment_->CreateMouseCursorMonitor(options), client_stub));
  pump->SetMouseCursorMonitorCallback(callback.get());
  return composer;
}

}  // namespace

// static
std::unique_ptr<ComposingCapturerCursorMonitorManager>
ComposingCapturerCursorMonitorManager::Create(
    DesktopEnvironment* desktop_environment) {
  return std::make_unique<ComposingCapturerCursorMonitorManagerWayland>(
      desktop_environment);
}

}  // namespace remoting
