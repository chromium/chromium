// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/process_selection_deferring_condition_runner.h"

#include "base/functional/callback.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "url/origin.h"

namespace content {

// static
std::unique_ptr<ProcessSelectionDeferringConditionRunner>
ProcessSelectionDeferringConditionRunner::Create(
    NavigationRequest& navigation_request) {
  std::unique_ptr<ProcessSelectionDeferringConditionRunner> runner =
      base::WrapUnique(new ProcessSelectionDeferringConditionRunner());
  runner->RegisterProcessSelectionDeferringConditions(navigation_request);
  return runner;
}

ProcessSelectionDeferringConditionRunner::
    ProcessSelectionDeferringConditionRunner() = default;

ProcessSelectionDeferringConditionRunner::
    ~ProcessSelectionDeferringConditionRunner() = default;

void ProcessSelectionDeferringConditionRunner::
    RegisterProcessSelectionDeferringConditions(
        NavigationRequest& navigation_request) {
  deferring_conditions_ =
      GetContentClient()
          ->browser()
          ->CreateProcessSelectionDeferringConditionsForNavigation(
              navigation_request);
}

void ProcessSelectionDeferringConditionRunner::OnRequestRedirected() {
  for (auto& deferring_condition : deferring_conditions_) {
    deferring_condition->OnRequestRedirected();
  }
}

void ProcessSelectionDeferringConditionRunner::WillSelectFinalProcess(
    base::OnceClosure on_completion_callback) {
  on_completion_callback_ = std::move(on_completion_callback);
  ProcessNextCondition();
}

void ProcessSelectionDeferringConditionRunner::ProcessNextCondition() {
  if (deferring_conditions_.empty()) {
    std::move(on_completion_callback_).Run();
    return;
  }

  auto resume_closure = base::BindOnce(
      &ProcessSelectionDeferringConditionRunner::ResumeProcessing,
      weak_factory_.GetWeakPtr());

  ProcessSelectionDeferringCondition* condition =
      deferring_conditions_.begin()->get();
  switch (condition->OnWillSelectFinalProcess(std::move(resume_closure))) {
    case ProcessSelectionDeferringCondition::Result::kDefer:
      // TODO(crbug.com/440164018): Add histograms to measure deferral metrics.
      return;
    case ProcessSelectionDeferringCondition::Result::kProceed:
      ResumeProcessing();
      return;
  }
}

void ProcessSelectionDeferringConditionRunner::ResumeProcessing() {
  CHECK(!deferring_conditions_.empty());
  deferring_conditions_.erase(deferring_conditions_.begin());
  ProcessNextCondition();
}

}  // namespace content
