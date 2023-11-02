// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_COST_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_COST_CHANGE_NOTIFIER_WIN_H_

#include <netlistmgr.h>
#include <wrl/client.h>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {
class NetworkCostManagerEventSinkWin;

// Uses the `INetworkCostManager` Windows OS API to monitor the cost of the
// current connection.  `INetworkCostManager` performs blocking IO and
// synchronous RPC and must be accessed through a thread pool COM STA single
// threaded task runner.  NetworkCostChangeNotifierWin uses
// `base::SequenceBound` to prevent these expensive operations from happening on
// the UI thread.
class NET_EXPORT_PRIVATE NetworkCostChangeNotifierWin final {
 public:
  using CostChangedCallback =
      base::RepeatingCallback<void(NetworkChangeNotifier::ConnectionCost)>;

  // Constructs a new instance using a new COM STA single threaded task runner
  // to post the task that creates NetworkCostChangeNotifierWin and subscribes
  // to cost change events.
  static base::SequenceBound<NetworkCostChangeNotifierWin> CreateInstance(
      CostChangedCallback cost_changed_callback);

  // Tests use this hook to provide a fake implementation of the OS APIs.
  // The fake implementation enables tests to simulate different cost values,
  // cost changed events and OS errors.
  using CoCreateInstanceCallback = base::RepeatingCallback<
      HRESULT(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*)>;
  static void OverrideCoCreateInstanceForTesting(
      CoCreateInstanceCallback callback_for_testing);

  NetworkCostChangeNotifierWin(const NetworkCostChangeNotifierWin&) = delete;
  NetworkCostChangeNotifierWin& operator=(const NetworkCostChangeNotifierWin&) =
      delete;

 private:
  friend class base::SequenceBound<NetworkCostChangeNotifierWin>;

  explicit NetworkCostChangeNotifierWin(
      CostChangedCallback cost_changed_callback);
  ~NetworkCostChangeNotifierWin();

  // Creates `INetworkCostManager` and subscribe to cost change events.
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
