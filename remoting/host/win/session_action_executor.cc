// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_action_executor.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/proto/action.pb.h"

namespace remoting {

using protocol::ActionRequest;

SessionActionExecutor::SessionActionExecutor(
    scoped_refptr<base::SingleThreadTaskRunner> execute_action_task_runner,
    const base::RepeatingClosure& inject_sas,
    const base::RepeatingClosure& lock_workstation)
    : execute_action_task_runner_(execute_action_task_runner),
      inject_sas_(inject_sas),
      lock_workstation_(lock_workstation) {}

SessionActionExecutor::~SessionActionExecutor() = default;

void SessionActionExecutor::ExecuteAction(const ActionRequest& request) {
  DCHECK(request.has_action());

  switch (request.action()) {
    case ActionRequest::SEND_ATTENTION_SEQUENCE:
      execute_action_task_runner_->PostTask(FROM_HERE, inject_sas_);
      break;

    case ActionRequest::LOCK_WORKSTATION:
      execute_action_task_runner_->PostTask(FROM_HERE, lock_workstation_);
      break;

    default:
      NOTREACHED() << "Unknown action type: " << request.action();
  }
}

}  // namespace remoting
