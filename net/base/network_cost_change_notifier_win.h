// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_COST_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_COST_CHANGE_NOTIFIER_WIN_H_

#include <netlistmgr.h>
#include <wrl/client.h>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/win/windows_version.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {
class NetworkCostManagerEventSinkWin;

// Uses the `INetworkCostManager` Windows OS API to monitor the cost of the
// current connection.  `INetworkCostManager` performs blocking IO and
// synchronous RPC, which must be accessed through a thread pool worker thread.
// `NetworkCostChangeNotifierWin` uses `base::SequenceBound` to prevent these
// expensive operations from happening on the UI thread.
class NET_EXPORT_PRIVATE NetworkCostChangeNotifierWin final {
 public:
  // `INetworkCostManager` requires Windows Build 19041 or higher.  On prior
  // builds, calls to the Windows OS API `IConnectionPoint::Advise()` may hang.
  static constexpr base::win::Version kSupportedOsVersion =
      base::win::Version::WIN10_20H1;

  using CostChangedCallback =
      base::RepeatingCallback<void(NetworkChangeNotifier::ConnectionCost)>;

  // Constructs a new instance using a COM STA single threaded task runner.
  // Posts the task that subscribes to cost change events using Windows OS APIs.
  static base::SequenceBound<NetworkCostChangeNotifierWin> CreateInstance(
      CostChangedCallback cost_changed_callback);

  // Use `CreateInstance()` above.  This constructor is public for use by
  // `base::SequenceBound` only.
  explicit NetworkCostChangeNotifierWin(
      CostChangedCallback cost_changed_callback);
  ~NetworkCostChangeNotifierWin();

  // Tests use this hook to provide a fake implementation of the OS APIs.
  // The fake implementation enables tests to simulate different network
  // conditions.
  using CoCreateInstanceCallback = base::RepeatingCallback<
      HRESULT(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*)>;
  static void OverrideCoCreateInstanceForTesting(
      CoCreateInstanceCallback callback_for_testing);

  NetworkCostChangeNotifierWin(const NetworkCostChangeNotifierWin&) = delete;
  NetworkCostChangeNotifierWin& operator=(const NetworkCostChangeNotifierWin&) =
      delete;

 private:
  // Creates `INetworkCostManager` for `cost_manager_` and subscribe to cost
  // change events.
  void StartWatching();

  // Stops monitoring the cost of the current connection by unsubscribing to
  // `INetworkCostManager` events and releasing all members.
  void StopWatching();

  // Gets the current cost from `cost_manager_` and then runs
  // `cost_changed_callback_`.
  void HandleCostChanged();

  // All members must be accessed on the sequence from `sequence_checker_`
  SEQUENCE_CHECKER(sequence_checker_);

  CostChangedCallback cost_changed_callback_;

  Microsoft::WRL::ComPtr<INetworkCostManager> cost_manager_;

  Microsoft::WRL::ComPtr<NetworkCostManagerEventSinkWin>
      cost_manager_event_sink_;

  base::WeakPtrFactory<NetworkCostChangeNotifierWin> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_BASE_NETWORK_COST_CHANGE_NOTIFIER_WIN_H_
