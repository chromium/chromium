// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_desktop_environment.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/basic_desktop_environment.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "remoting/host/host_window_proxy.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/errors.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#include "components/user_manager/user_manager.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/curtain_mode_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace remoting {

It2MeDesktopEnvironment::~It2MeDesktopEnvironment() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

It2MeDesktopEnvironment::It2MeDesktopEnvironment(
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

  // Create the local input monitor.
  local_input_monitor_ = LocalInputMonitor::Create(
      caller_task_runner, input_task_runner, ui_task_runner);
  local_input_monitor_->StartMonitoringForClientSession(client_session_control);

  bool enable_user_interface = options.enable_user_interface();
  bool enable_notifications = options.enable_notifications();
  // The host UI should be created on the UI thread.
#if BUILDFLAG(IS_APPLE)
  // Don't try to display any UI on top of the system's login screen as this
  // is rejected by the Window Server on OS X 10.7.4, and prevents the
  // capturer from working (http://crbug.com/140984).

  // TODO(lambroslambrou): Use a better technique of detecting whether we're
  // running in the LoginWindow context, and refactor this into a separate
  // function to be used here and in CurtainMode::ActivateCurtain().
  enable_user_interface = getuid() != 0;
#endif  // BUILDFLAG(IS_APPLE)

  // Create the continue window.  The implication of this window is that the
  // session length will be limited.  If the user interface is disabled,
  // then sessions will not have a maximum length enforced by the continue
  // window timer.
  if (enable_user_interface) {
    continue_window_ = HostWindow::CreateContinueWindow();
    continue_window_ = std::make_unique<HostWindowProxy>(
        caller_task_runner, ui_task_runner, std::move(continue_window_));
    continue_window_->Start(client_session_control);
  }

  // Create the disconnect window on Mac/Windows/Linux or a tray notification
  // on ChromeOS.  This has the effect of notifying the local user that
  // someone has remotely connected to their machine and providing them with
  // a disconnect button to terminate the connection.
  if (enable_notifications) {
    disconnect_window_ = HostWindow::CreateDisconnectWindow();
    disconnect_window_ = std::make_unique<HostWindowProxy>(
        caller_task_runner, ui_task_runner, std::move(disconnect_window_));
    disconnect_window_->Start(client_session_control);
  }
}

std::string It2MeDesktopEnvironment::GetCapabilities() const {
  std::string capabilities = BasicDesktopEnvironment::GetCapabilities();

  // TODO: joedow - Move MultiStream capability to a shared base
  // class once all platforms and connection modes support it.
  capabilities += " ";
  capabilities += protocol::kMultiStreamCapability;

  return capabilities;
}

It2MeDesktopEnvironmentFactory::It2MeDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     video_capture_task_runner,
                                     input_task_runner,
                                     ui_task_runner) {}

It2MeDesktopEnvironmentFactory::~It2MeDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment> It2MeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    const DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return base::WrapUnique(new It2MeDesktopEnvironment(
      caller_task_runner(), video_capture_task_runner(), input_task_runner(),
      ui_task_runner(), client_session_control, options));
}

}  // namespace remoting
