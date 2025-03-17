// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_WIN_SESSION_INTERACTION_STRATEGY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/legacy_interaction_strategy.h"

namespace remoting {

// Used to create audio/video capturers and event executor that are compatible
// with Windows sessions.
class SessionInteractionStrategy : public LegacyInteractionStrategy {
 public:
  SessionInteractionStrategy(SessionInteractionStrategy&&) = delete;
  SessionInteractionStrategy& operator=(SessionInteractionStrategy&&) = delete;

  ~SessionInteractionStrategy() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;

 private:
  SessionInteractionStrategy(
      const DesktopEnvironmentOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::RepeatingClosure inject_sas,
      base::RepeatingClosure lock_workstation);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to ask the daemon to inject Secure Attention Sequence.
  base::RepeatingClosure inject_sas_;

  // Used to lock the workstation for the current session.
  base::RepeatingClosure lock_workstation_;

  friend class SessionInteractionStrategyFactory;
};

// Used to create |SessionInteractionStrategy| instances.
class SessionInteractionStrategyFactory
    : public DesktopInteractionStrategyFactory {
 public:
  SessionInteractionStrategyFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::RepeatingClosure inject_sas,
      base::RepeatingClosure lock_workstation);

  SessionInteractionStrategyFactory(SessionInteractionStrategyFactory&&) =
      delete;
  SessionInteractionStrategyFactory& operator=(
      SessionInteractionStrategyFactory&&) = delete;

  ~SessionInteractionStrategyFactory() override;

  // SessionInteractionStrategyFactory implementation.
  void Create(const DesktopEnvironmentOptions& options,
              CreateCallback callback) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to ask the daemon to inject Secure Attention Sequence.
  base::RepeatingClosure inject_sas_;

  // Used to lock the workstation for the current session.
  base::RepeatingClosure lock_workstation_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_INTERACTION_STRATEGY_H_
