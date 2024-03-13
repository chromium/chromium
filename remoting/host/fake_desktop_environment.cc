// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_desktop_environment.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/fake_keyboard_layout_monitor.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/remote_open_url/fake_url_forwarder_configurator.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/fake_desktop_capturer.h"

namespace remoting {

FakeInputInjector::FakeInputInjector() {}
FakeInputInjector::~FakeInputInjector() = default;

void FakeInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {}

void FakeInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (key_events_) {
    key_events_->push_back(event);
  }
}

void FakeInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  if (text_events_) {
    text_events_->push_back(event);
  }
}

void FakeInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (mouse_events_) {
    mouse_events_->push_back(event);
  }
}

void FakeInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  if (touch_events_) {
    touch_events_->push_back(event);
  }
}

void FakeInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  if (clipboard_events_) {
    clipboard_events_->push_back(event);
  }
}

FakeScreenControls::FakeScreenControls() = default;
FakeScreenControls::~FakeScreenControls() = default;

void FakeScreenControls::SetScreenResolution(
    const ScreenResolution& resolution,
    std::optional<webrtc::ScreenId> screen_id) {}

void FakeScreenControls::SetVideoLayout(
    const protocol::VideoLayout& video_layout) {}

FakeDesktopEnvironment::FakeDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> capture_thread,
    const DesktopEnvironmentOptions& options)
    : capture_thread_(std::move(capture_thread)), options_(options) {}

FakeDesktopEnvironment::~FakeDesktopEnvironment() = default;

// DesktopEnvironment implementation.
std::unique_ptr<ActionExecutor> FakeDesktopEnvironment::CreateActionExecutor() {
  return nullptr;
}

std::unique_ptr<AudioCapturer> FakeDesktopEnvironment::CreateAudioCapturer() {
  return nullptr;
}

std::unique_ptr<InputInjector> FakeDesktopEnvironment::CreateInputInjector() {
  std::unique_ptr<FakeInputInjector> result(new FakeInputInjector());
  last_input_injector_ = result->weak_factory_.GetWeakPtr();
  return std::move(result);
}

std::unique_ptr<ScreenControls> FakeDesktopEnvironment::CreateScreenControls() {
  return std::make_unique<FakeScreenControls>();
}

std::unique_ptr<DesktopCapturer> FakeDesktopEnvironment::CreateVideoCapturer(
    webrtc::ScreenId id) {
  auto fake_capturer = std::make_unique<protocol::FakeDesktopCapturer>();
  if (!frame_generator_.is_null()) {
    fake_capturer->set_frame_generator(frame_generator_);
  }

  auto result = std::make_unique<DesktopCapturerProxy>(capture_thread_);
  result->set_capturer(std::move(fake_capturer));
  return std::move(result);
}

DesktopDisplayInfoMonitor* FakeDesktopEnvironment::GetDisplayInfoMonitor() {
  return nullptr;
}

std::unique_ptr<webrtc::MouseCursorMonitor>
FakeDesktopEnvironment::CreateMouseCursorMonitor() {
  return std::make_unique<FakeMouseCursorMonitor>();
}

std::unique_ptr<KeyboardLayoutMonitor>
FakeDesktopEnvironment::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return std::make_unique<FakeKeyboardLayoutMonitor>();
}

std::unique_ptr<ActiveDisplayMonitor>
FakeDesktopEnvironment::CreateActiveDisplayMonitor(
    ActiveDisplayMonitor::Callback callback) {
  auto result = std::make_unique<FakeActiveDisplayMonitor>(callback);
  last_active_display_monitor_ = result->GetWeakPtr();
  return result;
}

std::unique_ptr<FileOperations> FakeDesktopEnvironment::CreateFileOperations() {
  return nullptr;
}

std::unique_ptr<UrlForwarderConfigurator>
FakeDesktopEnvironment::CreateUrlForwarderConfigurator() {
  return std::make_unique<FakeUrlForwarderConfigurator>();
}

std::string FakeDesktopEnvironment::GetCapabilities() const {
  return capabilities_;
}

void FakeDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
  capabilities_ = capabilities;
}

uint32_t FakeDesktopEnvironment::GetDesktopSessionId() const {
  return desktop_session_id_;
}

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
FakeDesktopEnvironment::CreateRemoteWebAuthnStateChangeNotifier() {
  return nullptr;
}

const DesktopEnvironmentOptions& FakeDesktopEnvironment::options() const {
  return options_;
}

FakeDesktopEnvironmentFactory::FakeDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> capture_thread)
    : capture_thread_(std::move(capture_thread)) {}

FakeDesktopEnvironmentFactory::~FakeDesktopEnvironmentFactory() = default;

// DesktopEnvironmentFactory implementation.
std::unique_ptr<DesktopEnvironment> FakeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    const DesktopEnvironmentOptions& options) {
  std::unique_ptr<FakeDesktopEnvironment> result(
      new FakeDesktopEnvironment(capture_thread_, options));
  result->set_frame_generator(frame_generator_);
  result->set_desktop_session_id(desktop_session_id_);
  result->SetCapabilities(capabilities_);
  last_desktop_environment_ = result->weak_factory_.GetWeakPtr();
  return std::move(result);
}

bool FakeDesktopEnvironmentFactory::SupportsAudioCapture() const {
  return false;
}

}  // namespace remoting
