// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_interaction_strategy.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/legacy_interaction_strategy.h"
#include "remoting/host/win/session_action_executor.h"
#include "remoting/host/win/session_input_injector.h"

namespace remoting {

SessionInteractionStrategy::~SessionInteractionStrategy() = default;

std::unique_ptr<ActionExecutor>
SessionInteractionStrategy::CreateActionExecutor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<SessionActionExecutor>(
      caller_task_runner_, inject_sas_, lock_workstation_);
}

std::unique_ptr<InputInjector>
SessionInteractionStrategy::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<SessionInputInjectorWin>(
      input_task_runner_,
      InputInjector::Create(input_task_runner_, ui_task_runner_),
      ui_task_runner_, inject_sas_, lock_workstation_);
}

SessionInteractionStrategy::SessionInteractionStrategy(
    const DesktopEnvironmentOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::RepeatingClosure inject_sas,
    base::RepeatingClosure lock_workstation)
    : LegacyInteractionStrategy(options,
                                caller_task_runner,
                                ui_task_runner,
                                std::move(video_capture_task_runner),
                                input_task_runner),
      caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      input_task_runner_(input_task_runner),
      inject_sas_(std::move(inject_sas)),
      lock_workstation_(std::move(lock_workstation)) {}

SessionInteractionStrategyFactory::SessionInteractionStrategyFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::RepeatingClosure inject_sas,
    base::RepeatingClosure lock_workstation)
    : caller_task_runner_(std::move(caller_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      video_capture_task_runner_(std::move(video_capture_task_runner)),
      input_task_runner_(std::move(input_task_runner)),
      inject_sas_(std::move(inject_sas)),
      lock_workstation_(std::move(lock_workstation)) {}

SessionInteractionStrategyFactory::~SessionInteractionStrategyFactory() =
    default;

void SessionInteractionStrategyFactory::Create(
    const DesktopEnvironmentOptions& options,
    CreateCallback callback) {
  auto session = base::WrapUnique(new SessionInteractionStrategy(
      options, caller_task_runner_, ui_task_runner_, video_capture_task_runner_,
      input_task_runner_, inject_sas_, lock_workstation_));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(session)));
}

}  // namespace remoting
