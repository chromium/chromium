// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/test/mock_process_selection_deferring_condition_tester.h"

#include <utility>

#include "base/run_loop.h"
#include "content/public/browser/navigation_handle.h"

namespace content {
MockProcessSelectionDeferringCondition::MockProcessSelectionDeferringCondition(
    NavigationHandle& navigation_handle,
    ProcessSelectionDeferringCondition::Result result,
    OnRequestRedirectedCallback on_request_redirected_callback,
    OnWillSelectFinalProcessCallback on_will_select_final_process_callback)
    : ProcessSelectionDeferringCondition(navigation_handle),
      result_(result),
      on_request_redirected_callback_(
          std::move(on_request_redirected_callback)),
      on_will_select_final_process_callback_(
          std::move(on_will_select_final_process_callback)) {}

MockProcessSelectionDeferringCondition::
    ~MockProcessSelectionDeferringCondition() = default;

void MockProcessSelectionDeferringCondition::OnRequestRedirected() {
  if (on_request_redirected_callback_) {
    on_request_redirected_callback_.Run();
  }
}

ProcessSelectionDeferringCondition::Result
MockProcessSelectionDeferringCondition::OnWillSelectFinalProcess(
    base::OnceClosure resume) {
  if (on_will_select_final_process_callback_) {
    std::move(on_will_select_final_process_callback_).Run(std::move(resume));
  }
  return result_;
}

base::WeakPtr<MockProcessSelectionDeferringCondition>
MockProcessSelectionDeferringCondition::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

MockProcessSelectionDeferringConditionTester::
    MockProcessSelectionDeferringConditionTester(
        NavigationHandle& navigation_handle,
        ProcessSelectionDeferringCondition::Result result) {
  mock_deferring_condition_ =
      std::make_unique<MockProcessSelectionDeferringCondition>(
          navigation_handle, result,
          base::BindRepeating(&MockProcessSelectionDeferringConditionTester::
                                  OnRequestRedirectedCalled,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&MockProcessSelectionDeferringConditionTester::
                             OnWillSelectFinalProcessCalled,
                         weak_factory_.GetWeakPtr()));
  weak_condition_ = mock_deferring_condition_->AsWeakPtr();
}

MockProcessSelectionDeferringConditionTester::
    ~MockProcessSelectionDeferringConditionTester() = default;

int MockProcessSelectionDeferringConditionTester::
    GetOnRequestRedirectedCallCount() {
  return on_request_redirected_call_count_;
}

bool MockProcessSelectionDeferringConditionTester::
    WasOnWillSelectFinalProcessCalled() const {
  return did_call_on_will_select_process_;
}

void MockProcessSelectionDeferringConditionTester::OnRequestRedirectedCalled() {
  on_request_redirected_call_count_++;
}

void MockProcessSelectionDeferringConditionTester::
    OnWillSelectFinalProcessCalled(base::OnceClosure resume_closure) {
  did_call_on_will_select_process_ = true;
  resume_closure_ = std::move(resume_closure);
  if (on_will_select_final_process_closure_) {
    std::move(on_will_select_final_process_closure_).Run();
  }
}

std::unique_ptr<ProcessSelectionDeferringCondition>
MockProcessSelectionDeferringConditionTester::Release() {
  CHECK(mock_deferring_condition_);
  return std::move(mock_deferring_condition_);
}

void MockProcessSelectionDeferringConditionTester::CallResumeClosure() {
  CHECK(WasOnWillSelectFinalProcessCalled());
  CHECK(resume_closure_);
  std::move(resume_closure_).Run();
}

base::OnceClosure
MockProcessSelectionDeferringConditionTester::GetResumeClosure() {
  CHECK(WasOnWillSelectFinalProcessCalled());
  CHECK(resume_closure_);
  return std::move(resume_closure_);
}

void MockProcessSelectionDeferringConditionTester::
    WaitUntilOnWillSelectFinalProcessIsCalled() {
  if (WasOnWillSelectFinalProcessCalled()) {
    return;
  }
  base::RunLoop run_loop;
  on_will_select_final_process_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}
}  // namespace content
