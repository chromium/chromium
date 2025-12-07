// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_TEST_MOCK_PROCESS_SELECTION_DEFERRING_CONDITION_TESTER_H_
#define CONTENT_TEST_MOCK_PROCESS_SELECTION_DEFERRING_CONDITION_TESTER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/process_selection_deferring_condition.h"

namespace content {

class NavigationHandle;

// A mock ProcessSelectionDeferringCondition. This provides a stub
// implementation that can be used in tests to check that functions were called
// when expected. Test authors should use the
// MockProcessSelectionDeferringConditionTester below in their tests.
class MockProcessSelectionDeferringCondition
    : public ProcessSelectionDeferringCondition {
  using OnRequestRedirectedCallback = base::RepeatingCallback<void()>;
  using OnWillSelectFinalProcessCallback =
      base::OnceCallback<void(base::OnceClosure)>;

 public:
  explicit MockProcessSelectionDeferringCondition(
      NavigationHandle& navigation_handle,
      ProcessSelectionDeferringCondition::Result result,
      OnRequestRedirectedCallback on_request_redirected_callback,
      OnWillSelectFinalProcessCallback on_will_select_process_callback);
  ~MockProcessSelectionDeferringCondition() override;
  MockProcessSelectionDeferringCondition(
      const MockProcessSelectionDeferringCondition&) = delete;
  MockProcessSelectionDeferringCondition& operator=(
      const MockProcessSelectionDeferringCondition&) = delete;

  void OnRequestRedirected() override;

  ProcessSelectionDeferringCondition::Result OnWillSelectFinalProcess(
      base::OnceClosure resume) override;

  base::WeakPtr<MockProcessSelectionDeferringCondition> AsWeakPtr();

 private:
  ProcessSelectionDeferringCondition::Result result_;
  OnRequestRedirectedCallback on_request_redirected_callback_;
  OnWillSelectFinalProcessCallback on_will_select_final_process_callback_;
  base::WeakPtrFactory<MockProcessSelectionDeferringCondition> weak_factory_{
      this};
};

// A test helper for MockProcessSelectionDeferringConditions. This simplifies
// testing by owning the MockProcessSelectionDeferringCondition and providing
// an interface to observe and control its behavior.
class MockProcessSelectionDeferringConditionTester {
 public:
  explicit MockProcessSelectionDeferringConditionTester(
      NavigationHandle& navigation_handle,
      ProcessSelectionDeferringCondition::Result result);
  ~MockProcessSelectionDeferringConditionTester();

  int GetOnRequestRedirectedCallCount();

  bool WasOnWillSelectFinalProcessCalled() const;

  // Releases the wrapped condition.
  // `ProcessSelectionDeferringCondition` needs to be released so it can be
  // passed to a delegate. When this happens, the wrapper keeps a
  // `base::WeakPtr` reference to the condition so that it can still be notified
  // of changes to the condition.
  std::unique_ptr<ProcessSelectionDeferringCondition> Release();

  // Calls the `resume_closure` that was provided to the wrapped
  // `ProcessSelectionDeferringCondition`. This allows tests to check state
  // before simulating that the condition calls `resume`. This can only be
  // called after `OnWillSelectFinalProcess` is called on the condition. After
  // `CallResumeClosure()` is called, calling `GetResumeClosure()` will fail.
  void CallResumeClosure();

  // Returns the `resume_closure` that was provided to the wrapped
  // `ProcessSelectionDeferringCondition`. This allows tests to clearly
  // demonstrate that the callback is being called. This can only be called
  // after `OnWillSelectFinalProcess` is called on the condition. After
  // `GetResumeClosure()` is called, calling `CallResumeClosure()` will fail.
  base::OnceClosure GetResumeClosure();

  // Blocks until OnWillSelectFinalProcess() is called on the condition.
  void WaitUntilOnWillSelectFinalProcessIsCalled();

  bool IsDestroyed() { return !static_cast<bool>(weak_condition_); }

 private:
  // Handles when OnRequestRedirected is called on the wrapped condition.
  // This is supplied to the wrapped ProcessSelectionDeferringCondition and is
  // called when OnRequestRedirected is called.
  void OnRequestRedirectedCalled();

  // Handles when `OnWillSelectFinalProcess` is called on the wrapped condition.
  // This is supplied to the wrapped `ProcessSelectionDeferringCondition` and is
  // called when `OnWillSelectFinalProcess` is called. This allows tests to
  // inspect the wrapped condition.
  void OnWillSelectFinalProcessCalled(base::OnceClosure resume_closure);

  bool did_call_on_will_select_process_ = false;
  int on_request_redirected_call_count_ = 0;
  base::OnceClosure on_will_select_final_process_closure_;
  base::OnceClosure resume_closure_;

  std::unique_ptr<MockProcessSelectionDeferringCondition>
      mock_deferring_condition_;
  base::WeakPtr<MockProcessSelectionDeferringCondition> weak_condition_;
  base::WeakPtrFactory<MockProcessSelectionDeferringConditionTester>
      weak_factory_{this};
};
}  // namespace content
#endif  // CONTENT_TEST_MOCK_PROCESS_SELECTION_DEFERRING_CONDITION_TESTER_H_
