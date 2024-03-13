// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_active_display_monitor.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/input_injector.h"
#include "remoting/protocol/fake_desktop_capturer.h"

namespace remoting {

class FakeInputInjector : public InputInjector {
 public:
  FakeInputInjector();

  FakeInputInjector(const FakeInputInjector&) = delete;
  FakeInputInjector& operator=(const FakeInputInjector&) = delete;

  ~FakeInputInjector() override;

  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  void set_key_events(std::vector<protocol::KeyEvent>* key_events) {
    key_events_ = key_events;
  }
  void set_text_events(std::vector<protocol::TextEvent>* text_events) {
    text_events_ = text_events;
  }
  void set_mouse_events(std::vector<protocol::MouseEvent>* mouse_events) {
    mouse_events_ = mouse_events;
  }
  void set_touch_events(std::vector<protocol::TouchEvent>* touch_events) {
    touch_events_ = touch_events;
  }
  void set_clipboard_events(
      std::vector<protocol::ClipboardEvent>* clipboard_events) {
    clipboard_events_ = clipboard_events;
  }

 private:
  friend class FakeDesktopEnvironment;

  raw_ptr<std::vector<protocol::KeyEvent>> key_events_ = nullptr;
  raw_ptr<std::vector<protocol::TextEvent>> text_events_ = nullptr;
  raw_ptr<std::vector<protocol::MouseEvent>> mouse_events_ = nullptr;
  raw_ptr<std::vector<protocol::TouchEvent>> touch_events_ = nullptr;
  raw_ptr<std::vector<protocol::ClipboardEvent>> clipboard_events_ = nullptr;

  base::WeakPtrFactory<FakeInputInjector> weak_factory_{this};
};

class FakeScreenControls : public ScreenControls {
 public:
  FakeScreenControls();
  ~FakeScreenControls() override;

  // ScreenControls implementation.
  void SetScreenResolution(const ScreenResolution& resolution,
                           std::optional<webrtc::ScreenId> screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& video_layout) override;
};

class FakeDesktopEnvironment : public DesktopEnvironment {
 public:
  explicit FakeDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> capture_thread,
      const DesktopEnvironmentOptions& options);

  FakeDesktopEnvironment(const FakeDesktopEnvironment&) = delete;
  FakeDesktopEnvironment& operator=(const FakeDesktopEnvironment&) = delete;

  ~FakeDesktopEnvironment() override;

  // Sets frame generator to be used for protocol::FakeDesktopCapturer created
  // by FakeDesktopEnvironment.
  void set_frame_generator(
      protocol::FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = std::move(frame_generator);
  }

  void set_desktop_session_id(uint32_t desktop_session_id) {
    desktop_session_id_ = desktop_session_id;
  }

  const DesktopEnvironmentOptions& options() const;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) override;
  DesktopDisplayInfoMonitor* GetDisplayInfoMonitor() override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      ActiveDisplayMonitor::Callback callback) override;
  std::unique_ptr<FileOperations> CreateFileOperations() override;
  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator()
      override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;
  std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
  CreateRemoteWebAuthnStateChangeNotifier() override;

  base::WeakPtr<FakeInputInjector> last_input_injector() {
    return last_input_injector_;
  }

  base::WeakPtr<FakeActiveDisplayMonitor> last_active_display_monitor() {
    return last_active_display_monitor_;
  }

 private:
  friend class FakeDesktopEnvironmentFactory;

  scoped_refptr<base::SingleThreadTaskRunner> capture_thread_;
  protocol::FakeDesktopCapturer::FrameGenerator frame_generator_;
  uint32_t desktop_session_id_ = UINT32_MAX;

  base::WeakPtr<FakeInputInjector> last_input_injector_;
  base::WeakPtr<FakeActiveDisplayMonitor> last_active_display_monitor_;

  const DesktopEnvironmentOptions options_;

  std::string capabilities_;

  base::WeakPtrFactory<FakeDesktopEnvironment> weak_factory_{this};
};

class FakeDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  explicit FakeDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> capture_thread);

  FakeDesktopEnvironmentFactory(const FakeDesktopEnvironmentFactory&) = delete;
  FakeDesktopEnvironmentFactory& operator=(
      const FakeDesktopEnvironmentFactory&) = delete;

  ~FakeDesktopEnvironmentFactory() override;

  // Sets frame generator to be used for protocol::FakeDesktopCapturer created
  // by FakeDesktopEnvironment.
  void set_frame_generator(
      protocol::FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = std::move(frame_generator);
  }

  void set_desktop_session_id(uint32_t desktop_session_id) {
    desktop_session_id_ = desktop_session_id;
  }

  // Sets the capabilities that the FakeDesktopEnvironment will claim to
  // support. Useful for testing functionality that is triggered after
  // negotiating a capability with a client.
  void set_capabilities(const std::string& capabilities) {
    capabilities_ = capabilities;
  }

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      const DesktopEnvironmentOptions& options) override;
  bool SupportsAudioCapture() const override;

  base::WeakPtr<FakeDesktopEnvironment> last_desktop_environment() {
    return last_desktop_environment_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> capture_thread_;
  protocol::FakeDesktopCapturer::FrameGenerator frame_generator_;
  uint32_t desktop_session_id_ = UINT32_MAX;
  std::string capabilities_;

  base::WeakPtr<FakeDesktopEnvironment> last_desktop_environment_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
