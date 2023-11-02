// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_

#include "base/task/single_thread_task_runner.h"
#include "remoting/host/basic_desktop_environment.h"

namespace remoting {

class CurtainMode;
class HostWindow;
class LocalInputMonitor;

// Same as BasicDesktopEnvironment but supports desktop resizing and X DAMAGE
// notifications on Linux.
class Me2MeDesktopEnvironment : public BasicDesktopEnvironment {
 public:
  Me2MeDesktopEnvironment(const Me2MeDesktopEnvironment&) = delete;
  Me2MeDesktopEnvironment& operator=(const Me2MeDesktopEnvironment&) = delete;

  ~Me2MeDesktopEnvironment() override;

  // DesktopEnvironment interface.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::string GetCapabilities() const override;

 protected:
  friend class Me2MeDesktopEnvironmentFactory;
  Me2MeDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options);

  // Initializes security features of the desktop environment (the curtain mode
  // and in-session UI).
  bool InitializeSecurity(
      base::WeakPtr<ClientSessionControl> client_session_control);

 private:
  // "Curtains" the session making sure it is disconnected from the local
  // console.
  std::unique_ptr<CurtainMode> curtain_;

  // Presents the disconnect window to the local user.
  std::unique_ptr<HostWindow> disconnect_window_;

  // Notifies the client session about the local mouse movements.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;
};

// Used to create |Me2MeDesktopEnvironment| instances.
class Me2MeDesktopEnvironmentFactory : public BasicDesktopEnvironmentFactory {
 public:
  Me2MeDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  Me2MeDesktopEnvironmentFactory(const Me2MeDesktopEnvironmentFactory&) =
      delete;
  Me2MeDesktopEnvironmentFactory& operator=(
      const Me2MeDesktopEnvironmentFactory&) = delete;

  ~Me2MeDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory interface.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      const DesktopEnvironmentOptions& options) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
