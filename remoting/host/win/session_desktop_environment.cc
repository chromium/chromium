// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_desktop_environment.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/win/session_action_executor.h"
#include "remoting/host/win/session_input_injector.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

SessionDesktopEnvironment::~SessionDesktopEnvironment() = default;

std::unique_ptr<ActionExecutor>
SessionDesktopEnvironment::CreateActionExecutor() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return std::make_unique<SessionActionExecutor>(
      caller_task_runner(), inject_sas_, lock_workstation_);
}

std::unique_ptr<InputInjector>
SessionDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return std::make_unique<SessionInputInjectorWin>(
      input_task_runner(),
      InputInjector::Create(input_task_runner(), ui_task_runner()),
      ui_task_runner(), inject_sas_, lock_workstation_);
}

SessionDesktopEnvironment::SessionDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    const base::RepeatingClosure& inject_sas,
    const base::RepeatingClosure& lock_workstation,
    const DesktopEnvironmentOptions& options)
    : Me2MeDesktopEnvironment(caller_task_runner,
                              video_capture_task_runner,
                              input_task_runner,
                              ui_task_runner,
                              client_session_control,
                              options),
      inject_sas_(inject_sas),
      lock_workstation_(lock_workstation) {}

SessionDesktopEnvironmentFactory::SessionDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const base::RepeatingClosure& inject_sas,
    const base::RepeatingClosure& lock_workstation)
    : Me2MeDesktopEnvironmentFactory(caller_task_runner,
                                     video_capture_task_runner,
                                     input_task_runner,
                                     ui_task_runner),
      inject_sas_(inject_sas),
      lock_workstation_(lock_workstation) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

SessionDesktopEnvironmentFactory::~SessionDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment> SessionDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  std::unique_ptr<SessionDesktopEnvironment> desktop_environment(
      new SessionDesktopEnvironment(
          caller_task_runner(), video_capture_task_runner(),
          input_task_runner(), ui_task_runner(), client_session_control,
          inject_sas_, lock_workstation_, options));
  if (!desktop_environment->InitializeSecurity(client_session_control)) {
    return nullptr;
  }

  return std::move(desktop_environment);
}

}  // namespace remoting
