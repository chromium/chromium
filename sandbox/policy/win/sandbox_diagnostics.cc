// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_diagnostics.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"

namespace sandbox {
namespace policy {
namespace {
// Runs on a non-sandbox thread to ensure that response callback is not
// invoked from sandbox process and job tracker thread, and that conversion
// work does not block process or job registration. Converts |policies|
// into base::Value form, then invokes |response| on the same sequence.
static void ConvertToValuesAndRespond(
    std::unique_ptr<PolicyList> policies,
    base::OnceCallback<void(base::Value)> response) {
  base::Value::List policy_values;
  for (auto&& item : *policies) {
    auto snapshot = base::JSONReader::ReadAndReturnValueWithError(
        item->JsonString(), base::JSON_PARSE_RFC);
    CHECK(snapshot.has_value());
    policy_values.Append(std::move(*snapshot));
  }
  std::move(response).Run(base::Value(std::move(policy_values)));
}

// Runs on a non-sandbox thread to ensure that response callback is not
// invoked from sandbox process and job tracker thread.
static void RespondWithEmptyList(
    base::OnceCallback<void(base::Value)> response) {
  base::Value empty(base::Value::Type::LIST);
  std::move(response).Run(std::move(empty));
}

}  // namespace

ServiceManagerDiagnosticsReceiver::ServiceManagerDiagnosticsReceiver(
    scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
    base::OnceCallback<void(base::Value)> response)
    : response_(std::move(response)), origin_task_runner_(origin_task_runner) {}

ServiceManagerDiagnosticsReceiver::~ServiceManagerDiagnosticsReceiver() {}

// This is called by the sandbox's process and job tracking thread and must
// return quickly.
void ServiceManagerDiagnosticsReceiver::ReceiveDiagnostics(
    std::unique_ptr<PolicyList> policies) {
  // Need to run the conversion work on the origin thread.
  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConvertToValuesAndRespond, std::move(policies),
                                std::move(response_)));
}

// This is called by the sandbox's process and job tracking thread and must
// return quickly so we post to the origin thread.
void ServiceManagerDiagnosticsReceiver::OnError(ResultCode error) {
  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RespondWithEmptyList, std::move(response_)));
}

}  // namespace policy
}  // namespace sandbox
