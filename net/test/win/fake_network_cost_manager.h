// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_WIN_FAKE_NETWORK_COST_MANAGER_H_
#define NET_TEST_WIN_FAKE_NETWORK_COST_MANAGER_H_

#include <windows.h>

#include <wrl/client.h>

#include <vector>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "net/base/network_change_notifier.h"

namespace net {
class FakeNetworkCostManager;

// Each value represents a different Windows OS API that can fail when
// monitoring the cost of network connections.  Use with
// `FakeNetworkCostManagerEnvironment::SimulateError()` to simulate Windows OS
// API failures that return error `HRESULT` codes.
enum class NetworkCostManagerStatus {
  kOk,
  kErrorCoCreateInstanceFailed,
  kErrorQueryInterfaceFailed,
  kErrorFindConnectionPointFailed,
  kErrorAdviseFailed,
  kErrorGetCostFailed,
};

// Provides a fake implementation of the `INetworkCostManager` Windows OS API
// for `NetworkCostChangeNotifierWin`.  Must be constructed before any
// `NetworkCostChangeNotifierWin` instances exist.  Sets up the fake OS API in
// the constructor and restores the real OS API in the destructor.  Tests should
// use this class to simulate different network costs, cost changed events and
// errors without depending on the actual OS APIs or current network
// environment.
class FakeNetworkCostManagerEnvironment {
 public:
  FakeNetworkCostManagerEnvironment();
  ~FakeNetworkCostManagerEnvironment();

  void SetCost(NetworkChangeNotifier::ConnectionCost value);
  void SimulateError(NetworkCostManagerStatus error_status);

  FakeNetworkCostManagerEnvironment(const FakeNetworkCostManagerEnvironment&) =
      delete;
  FakeNetworkCostManagerEnvironment& operator=(
      const FakeNetworkCostManagerEnvironment&) = delete;

 private:
  // Creates a fake implementation of `INetworkCostManager`.
  HRESULT FakeCoCreateInstance(REFCLSID class_id,
                               LPUNKNOWN outer_aggregate,
                               DWORD context_flags,
                               REFIID interface_id,
                               LPVOID* result);

  // Members must be accessed while holding this lock to support the creation
  // and use of `FakeNetworkCostManager` instances on any thread.
  base::Lock member_lock_;

  // The connection cost to simulate.
  NetworkChangeNotifier::ConnectionCost connection_cost_
      GUARDED_BY(member_lock_) =
          NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN;

  // When `FakeNetworkCostManagerEnvironment` creates a new
  // `FakeNetworkCostManager`, the new `FakeNetworkCostManager` will simulate
  // this error.
  NetworkCostManagerStatus error_status_ GUARDED_BY(member_lock_) =
      NetworkCostManagerStatus::kOk;

  // Holds the fake implementations of `INetworkCostManager` constructed through
  // `FakeCoCreateInstance()`.
  std::vector<Microsoft::WRL::ComPtr<FakeNetworkCostManager>>
      fake_network_cost_managers_ GUARDED_BY(member_lock_);
};

}  // namespace net

#endif  // NET_TEST_WIN_FAKE_NETWORK_COST_MANAGER_H_
