// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PROCESS_SELECTION_DEFERRING_CONDITION_RUNNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PROCESS_SELECTION_DEFERRING_CONDITION_RUNNER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {
class NavigationRequest;
class ProcessSelectionDeferringCondition;

// Helper class to defer renderer process selection until dependencies are
// ready.
//
// Clients subclass the `ProcessSelectionDeferringCondition` class in order to
// initiate asynchronous tasks at navigation start, update the tasks on
// redirects, and be given the opportunity to defer the selection of the
// renderer process until the condition's tasks have completed. The client
// should register their subclass in
// `RegisterProcessSelectionDeferringConditions`, or in the embedder's
// `ContentBrowserClient::CreateProcessSelectionDeferringConditionsForNavigation()`
// method.
//
// The mechanism is not applied for prerender activations or about:blank or
// same-document navigations.
//
// TODO(crbug.com/439013866): add support for tracing.
class CONTENT_EXPORT ProcessSelectionDeferringConditionRunner {
 public:
  // Creates the runner, asks the embedder for any conditions that it wishes to
  // register, and then starts the conditions.
  static std::unique_ptr<ProcessSelectionDeferringConditionRunner> Create(
      NavigationRequest& navigation_request);

  ProcessSelectionDeferringConditionRunner(
      const ProcessSelectionDeferringConditionRunner&) = delete;
  ProcessSelectionDeferringConditionRunner& operator=(
      const ProcessSelectionDeferringConditionRunner&) = delete;

  ~ProcessSelectionDeferringConditionRunner();

  // Called by NavigationRequest::OnRequestRedirected(). This calls
  // OnRequestRedirected() on each of the conditions that have been registered.
  void OnRequestRedirected();

  // Called prior to process selection. This calls
  // `ProcessSelectionDeferringCondition::OnWillSelectFinalProcess()` on each
  // registered condition. Registered conditions are given the opportunity to
  // defer the process selection.
  void WillSelectFinalProcess(base::OnceClosure on_completion_callback);

 private:
  friend class ProcessSelectionDeferringConditionRunnerTest;

  ProcessSelectionDeferringConditionRunner();

  // Registers `ProcessSelectionDeferringCondition` instances that will be
  // processed during the navigation.
  void RegisterProcessSelectionDeferringConditions(
      NavigationRequest& navigation_request);

  // Processes the next condition on the list.
  void ProcessNextCondition();

  // Called asynchronously by `RegisterProcessSelectionDeferringConditions` to
  // continue processing after deferring.
  void ResumeProcessing();

  std::vector<std::unique_ptr<ProcessSelectionDeferringCondition>>
      deferring_conditions_;

  // Called after all conditions have completed.
  base::OnceClosure on_completion_callback_;

  base::WeakPtrFactory<ProcessSelectionDeferringConditionRunner> weak_factory_{
      this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PROCESS_SELECTION_DEFERRING_CONDITION_RUNNER_H_
