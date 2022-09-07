// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/conditional_composer_mouse_cursor_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "remoting/host/mouse_shape_pump.h"

namespace remoting {

namespace {

// This is the default implementation of the manager class which allows creation
// of multiple composers along with a single mouse shape pump monitor to go with
// them.
class ComposingCapturerCursorMonitorManagerDefault
    : public ComposingCapturerCursorMonitorManager,
      public webrtc::MouseCursorMonitor::Callback {
 public:
  explicit ComposingCapturerCursorMonitorManagerDefault(
      DesktopEnvironment* desktop_environment);

  // ComposingCapturerCursorMonitorManager implementation.
  std::unique_ptr<DesktopAndCursorConditionalComposer>
  CreateComposingVideoCapturer(protocol::ClientStub* client_stub) override;

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

 private:
  base::raw_ptr<DesktopEnvironment> desktop_environment_ = nullptr;
  std::vector<base::WeakPtr<DesktopAndCursorConditionalComposer>> composers_;
  std::unique_ptr<MouseShapePump> mouse_shape_pump_;
};

ComposingCapturerCursorMonitorManagerDefault::
    ComposingCapturerCursorMonitorManagerDefault(
        DesktopEnvironment* desktop_environment)
    : desktop_environment_(desktop_environment) {}

std::unique_ptr<DesktopAndCursorConditionalComposer>
ComposingCapturerCursorMonitorManagerDefault::CreateComposingVideoCapturer(
    protocol::ClientStub* client_stub) {
  auto composer = std::make_unique<DesktopAndCursorConditionalComposer>(
      desktop_environment_->CreateVideoCapturer(
          desktop_environment_->CaptureOptions()));
  composers_.push_back(composer->GetWeakPtr());

  // Create a mouse shape pump if one doesn't exist already.
  if (!mouse_shape_pump_) {
    mouse_shape_pump_ = std::make_unique<MouseShapePump>(
        desktop_environment_->CreateMouseCursorMonitor(
            desktop_environment_->CaptureOptions()),
        client_stub);
    mouse_shape_pump_->SetMouseCursorMonitorCallback(this);
  }

  return composer;
}

void ComposingCapturerCursorMonitorManagerDefault::OnMouseCursor(
    webrtc::MouseCursor* mouse_cursor) {
  // This method should take ownership of |mouse_cursor|.
  std::unique_ptr<webrtc::MouseCursor> owned_cursor(mouse_cursor);
  for (const auto& composer : composers_) {
    if (composer) {
      composer->SetMouseCursor(
          base::WrapUnique(webrtc::MouseCursor::CopyOf(*owned_cursor)));
    }
  }
}

void ComposingCapturerCursorMonitorManagerDefault::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  for (const auto& composer : composers_) {
    if (composer) {
      composer->SetMouseCursorPosition(position);
    }
  }
}

}  // namespace

// static
std::unique_ptr<ComposingCapturerCursorMonitorManager>
ComposingCapturerCursorMonitorManager::Create(
    DesktopEnvironment* desktop_environment) {
  return std::make_unique<ComposingCapturerCursorMonitorManagerDefault>(
      desktop_environment);
}

}  // namespace remoting
