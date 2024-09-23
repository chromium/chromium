// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_connection_cost_observer.h"

namespace net {

TestConnectionCostObserver::TestConnectionCostObserver() = default;

TestConnectionCostObserver::~TestConnectionCostObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TestConnectionCostObserver::OnConnectionCostChanged(
    NetworkChangeNotifier::ConnectionCost cost) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cost_changed_inputs_.push_back(cost);

  if (run_loop_) {
    run_loop_->Quit();
  }
}

void TestConnectionCostObserver::WaitForConnectionCostChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

size_t TestConnectionCostObserver::cost_changed_calls() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cost_changed_inputs_.size();
}

std::vector<NetworkChangeNotifier::ConnectionCost>
TestConnectionCostObserver::cost_changed_inputs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cost_changed_inputs_;
}

NetworkChangeNotifier::ConnectionCost
TestConnectionCostObserver::last_cost_changed_input() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GT(cost_changed_inputs_.size(), 0u);
  return cost_changed_inputs_.back();
}

}  // namespace net
