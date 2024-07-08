// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_CONNECTION_COST_OBSERVER_H_
#define NET_TEST_TEST_CONNECTION_COST_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "net/base/network_change_notifier.h"

namespace net {

class TestConnectionCostObserver final
    : public NetworkChangeNotifier::ConnectionCostObserver {
 public:
  TestConnectionCostObserver();
  ~TestConnectionCostObserver() final;

  void OnConnectionCostChanged(
      NetworkChangeNotifier::ConnectionCost cost) final;

  void WaitForConnectionCostChanged();

  size_t cost_changed_calls() const;
  std::vector<NetworkChangeNotifier::ConnectionCost> cost_changed_inputs()
      const;
  NetworkChangeNotifier::ConnectionCost last_cost_changed_input() const;

  TestConnectionCostObserver(const TestConnectionCostObserver&) = delete;
  TestConnectionCostObserver& operator=(const TestConnectionCostObserver&) =
      delete;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Set and used to block in `WaitForConnectionCostChanged()` until the next
  // cost changed event occurs.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Record each `OnConnectionCostChanged()` call.
  std::vector<NetworkChangeNotifier::ConnectionCost> cost_changed_inputs_;
};

}  // namespace net

#endif  // NET_TEST_TEST_CONNECTION_COST_OBSERVER_H_
