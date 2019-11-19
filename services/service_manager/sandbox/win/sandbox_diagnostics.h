// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_WIN_SANDBOX_DIAGNOSTICS_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_WIN_SANDBOX_DIAGNOSTICS_H_

#include "services/service_manager/sandbox/win/sandbox_win.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "sandbox/constants.h"
#include "sandbox/win/src/sandbox.h"

namespace service_manager {

// Mediates response from sandbox::BrokerServices->GetPolicyDiagnostics.
class ServiceManagerDiagnosticsReceiver
    : public sandbox::PolicyDiagnosticsReceiver {
 public:
  ~ServiceManagerDiagnosticsReceiver() final;
  ServiceManagerDiagnosticsReceiver(
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::OnceCallback<void(base::Value)> response);

  // This is called by the sandbox's process and job tracking thread and must
  // return quickly.
  void ReceiveDiagnostics(
      std::unique_ptr<sandbox::PolicyList> policies) override;

  // This is called by the sandbox's process and job tracking thread and must
  // return quickly.
  void OnError(sandbox::ResultCode error) override;

 private:
  base::OnceCallback<void(base::Value)> response_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
};
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_WIN_SANDBOX_DIAGNOSTICS_H_
