// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_DIAGNOSTICS_H_
#define SANDBOX_POLICY_WIN_SANDBOX_DIAGNOSTICS_H_

#include "sandbox/policy/win/sandbox_win.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "sandbox/constants.h"
#include "sandbox/win/src/sandbox.h"

namespace sandbox {
namespace policy {

// Mediates response from BrokerServices->GetPolicyDiagnostics.
class ServiceManagerDiagnosticsReceiver final
    : public PolicyDiagnosticsReceiver {
 public:
  ~ServiceManagerDiagnosticsReceiver() override;
  ServiceManagerDiagnosticsReceiver(
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::OnceCallback<void(base::Value)> response);

  // This is called by the sandbox's process and job tracking thread and must
  // return quickly.
  void ReceiveDiagnostics(std::unique_ptr<PolicyList> policies) override;

  // This is called by the sandbox's process and job tracking thread and must
  // return quickly.
  void OnError(ResultCode error) override;

 private:
  base::OnceCallback<void(base::Value)> response_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
};
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_SANDBOX_DIAGNOSTICS_H_
