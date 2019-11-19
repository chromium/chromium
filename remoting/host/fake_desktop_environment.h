// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/protocol/fake_desktop_capturer.h"

namespace remoting {

class FakeInputInjector : public InputInjector {
 public:
  FakeInputInjector();
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

  std::vector<protocol::KeyEvent>* key_events_ = nullptr;
  std::vector<protocol::TextEvent>* text_events_ = nullptr;
  std::vector<protocol::MouseEvent>* mouse_events_ = nullptr;
  std::vector<protocol::TouchEvent>* touch_events_ = nullptr;
  std::vector<protocol::ClipboardEvent>* clipboard_events_ = nullptr;

  base::WeakPtrFactory<FakeInputInjector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeInputInjector);
};

class FakeScreenControls : public ScreenControls {
 public:
  FakeScreenControls();
  ~FakeScreenControls() override;

  // ScreenControls implementation.
  void SetScreenResolution(const ScreenResolution& resolution) override;
};

class FakeDesktopEnvironment : public DesktopEnvironment {
 public:
  explicit FakeDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> capture_thread,
      const DesktopEnvironmentOptions& options);
  ~FakeDesktopEnvironment() override;

  // Sets frame generator to be used for protocol::FakeDesktopCapturer created
  // by FakeDesktopEnvironment.
  void set_frame_generator(
      protocol::FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  const DesktopEnvironmentOptions& options() const;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<FileOperations> CreateFileOperations() override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;

  base::WeakPtr<FakeInputInjector> last_input_injector() {
    return last_input_injector_;
  }

 private:
  friend class FakeDesktopEnvironmentFactory;

  scoped_refptr<base::SingleThreadTaskRunner> capture_thread_;
  protocol::FakeDesktopCapturer::FrameGenerator frame_generator_;

  base::WeakPtr<FakeInputInjector> last_input_injector_;

  const DesktopEnvironmentOptions options_;

  base::WeakPtrFactory<FakeDesktopEnvironment> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironment);
};

class FakeDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  explicit FakeDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> capture_thread);
  ~FakeDesktopEnvironmentFactory() override;

  // Sets frame generator to be used for protocol::FakeDesktopCapturer created
  // by FakeDesktopEnvironment.
  void set_frame_generator(
      protocol::FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options) override;
  bool SupportsAudioCapture() const override;

  base::WeakPtr<FakeDesktopEnvironment> last_desktop_environment() {
    return last_desktop_environment_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> capture_thread_;
  protocol::FakeDesktopCapturer::FrameGenerator frame_generator_;

  base::WeakPtr<FakeDesktopEnvironment> last_desktop_environment_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
