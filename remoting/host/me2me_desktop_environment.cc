// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_desktop_environment.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/host_window.h"
#include "remoting/host/host_window_proxy.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/screen_controls.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace remoting {

Me2MeDesktopEnvironment::~Me2MeDesktopEnvironment() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

std::unique_ptr<ActionExecutor>
Me2MeDesktopEnvironment::CreateActionExecutor() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return ActionExecutor::Create();
}

std::unique_ptr<ScreenControls>
Me2MeDesktopEnvironment::CreateScreenControls() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // We only want to restore the host resolution on disconnect if we are not
  // curtained so we don't mess up the user's window layout unnecessarily if
  // they disconnect and reconnect. Both OS X and Windows will restore the
  // resolution automatically when the user logs back in on the console, and on
  // Linux the curtain-mode uses a separate session.
  return base::WrapUnique(new ResizingHostObserver(DesktopResizer::Create(),
                                                   curtain_ == nullptr));
}

std::string Me2MeDesktopEnvironment::GetCapabilities() const {
  std::string capabilities;
  capabilities += protocol::kRateLimitResizeRequests;
  if (InputInjector::SupportsTouchEvents()) {
    capabilities += " ";
    capabilities += protocol::kTouchEventsCapability;
  }

  if (desktop_environment_options().enable_file_transfer()) {
    capabilities += " ";
    capabilities += protocol::kFileTransferCapability;
  }

#if defined(OS_WIN)
  capabilities += " ";
  capabilities += protocol::kSendAttentionSequenceAction;

  if (base::win::OSInfo::GetInstance()->version_type() !=
      base::win::VersionType::SUITE_HOME) {
    capabilities += " ";
    capabilities += protocol::kLockWorkstationAction;
  }
#endif  // defined(OS_WIN)

  return capabilities;
}

Me2MeDesktopEnvironment::Me2MeDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options)
    : BasicDesktopEnvironment(caller_task_runner,
                              video_capture_task_runner,
                              input_task_runner,
                              ui_task_runner,
                              client_session_control,
                              options) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());

  // TODO(zijiehe): This logic should belong to RemotingMe2MeHost, instead of
  // Me2MeDesktopEnvironment, which does not take response to create a new
  // session.
  // X DAMAGE is not enabled by default, since it is broken on many systems -
  // see http://crbug.com/73423. It's safe to enable it here because it works
  // properly under Xvfb.
  mutable_desktop_capture_options()->set_use_update_notifications(true);
}

bool Me2MeDesktopEnvironment::InitializeSecurity(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Detach the session from the local console if the caller requested.
  if (desktop_environment_options().enable_curtaining()) {
    curtain_ = CurtainMode::Create(
        caller_task_runner(), ui_task_runner(), client_session_control);
    if (!curtain_->Activate()) {
      LOG(ERROR) << "Failed to activate the curtain mode.";
      curtain_ = nullptr;
      return false;
    }
    return true;
  }

  // Otherwise, if the session is shared with the local user start monitoring
  // the local input and create the in-session UI.
#if defined(OS_LINUX)
  bool want_user_interface = false;
#elif defined(OS_MACOSX)
  // Don't try to display any UI on top of the system's login screen as this
  // is rejected by the Window Server on OS X 10.7.4, and prevents the
  // capturer from working (http://crbug.com/140984).

  // TODO(lambroslambrou): Use a better technique of detecting whether we're
  // running in the LoginWindow context, and refactor this into a separate
  // function to be used here and in CurtainMode::ActivateCurtain().
  bool want_user_interface = getuid() != 0;
#else
  bool want_user_interface =
      desktop_environment_options().enable_user_interface();
#endif

  if (want_user_interface) {
    // Create the local input monitor.
    local_input_monitor_ = LocalInputMonitor::Create(
        caller_task_runner(), input_task_runner(), ui_task_runner());
    local_input_monitor_->StartMonitoringForClientSession(
        client_session_control);

    // Create the disconnect window.
#if defined(OS_WIN)
    disconnect_window_ =
        HostWindow::CreateAutoHidingDisconnectWindow(LocalInputMonitor::Create(
            caller_task_runner(), input_task_runner(), ui_task_runner()));
#else
    disconnect_window_ = HostWindow::CreateDisconnectWindow();
#endif
    disconnect_window_.reset(new HostWindowProxy(
        caller_task_runner(), ui_task_runner(), std::move(disconnect_window_)));
    disconnect_window_->Start(client_session_control);
  }

  return true;
}

Me2MeDesktopEnvironmentFactory::Me2MeDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     video_capture_task_runner,
                                     input_task_runner,
                                     ui_task_runner) {}

Me2MeDesktopEnvironmentFactory::~Me2MeDesktopEnvironmentFactory() {
}

std::unique_ptr<DesktopEnvironment> Me2MeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  std::unique_ptr<Me2MeDesktopEnvironment> desktop_environment(
      new Me2MeDesktopEnvironment(caller_task_runner(),
                                  video_capture_task_runner(),
                                  input_task_runner(), ui_task_runner(),
                                  client_session_control, options));
  if (!desktop_environment->InitializeSecurity(client_session_control)) {
    return nullptr;
  }

  return std::move(desktop_environment);
}

}  // namespace remoting
