// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_result_queue.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

ScriptResultQueue::ScriptResultQueue() {
  test_api_observation_.Observe(
      extensions::TestApiObserverRegistry::GetInstance());
}
ScriptResultQueue::~ScriptResultQueue() = default;

void ScriptResultQueue::OnScriptResult(const base::Value& script_result) {
  results_.Append(script_result.Clone());
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

base::Value ScriptResultQueue::GetNextResult() {
  if (next_result_index_ >= results_.size()) {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  if (next_result_index_ >= results_.size()) {
    // This can happen if the run loop times out. Handle it gracefully to avoid
    // crashing the test runner.
    ADD_FAILURE() << "Could not get next result at index "
                  << next_result_index_;
    return base::Value();
  }

  return results_[next_result_index_++].Clone();
}

}  // namespace extensions
