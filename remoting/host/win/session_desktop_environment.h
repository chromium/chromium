// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/me2me_desktop_environment.h"

namespace remoting {

// Used to create audio/video capturers and event executor that are compatible
// with Windows sessions.
class SessionDesktopEnvironment : public Me2MeDesktopEnvironment {
 public:
  SessionDesktopEnvironment(const SessionDesktopEnvironment&) = delete;
  SessionDesktopEnvironment& operator=(const SessionDesktopEnvironment&) =
      delete;

  ~SessionDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;

 private:
  friend class SessionDesktopEnvironmentFactory;
  SessionDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      const base::RepeatingClosure& inject_sas,
      const base::RepeatingClosure& lock_workstation,
      const DesktopEnvironmentOptions& options);

  // Used to ask the daemon to inject Secure Attention Sequence.
  base::RepeatingClosure inject_sas_;

  // Used to lock the workstation for the current session.
  base::RepeatingClosure lock_workstation_;
};

// Used to create |SessionDesktopEnvironment| instances.
class SessionDesktopEnvironmentFactory : public Me2MeDesktopEnvironmentFactory {
 public:
  SessionDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const base::RepeatingClosure& inject_sas,
      const base::RepeatingClosure& lock_workstation);

  SessionDesktopEnvironmentFactory(const SessionDesktopEnvironmentFactory&) =
      delete;
  SessionDesktopEnvironmentFactory& operator=(
      const SessionDesktopEnvironmentFactory&) = delete;

  ~SessionDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      const DesktopEnvironmentOptions& options) override;

 private:
  // Used to ask the daemon to inject Secure Attention Sequence.
  base::RepeatingClosure inject_sas_;

  // Used to lock the workstation for the current session.
  base::RepeatingClosure lock_workstation_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
