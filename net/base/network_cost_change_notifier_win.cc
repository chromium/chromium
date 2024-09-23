// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_cost_change_notifier_win.h"

#include <wrl.h>
#include <wrl/client.h>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/com_init_util.h"

using Microsoft::WRL::ComPtr;

namespace net {

namespace {

NetworkChangeNotifier::ConnectionCost ConnectionCostFromNlmConnectionCost(
    DWORD connection_cost_flags) {
  if (connection_cost_flags == NLM_CONNECTION_COST_UNKNOWN) {
    return NetworkChangeNotifier::CONNECTION_COST_UNKNOWN;
  } else if ((connection_cost_flags & NLM_CONNECTION_COST_UNRESTRICTED) != 0) {
    return NetworkChangeNotifier::CONNECTION_COST_UNMETERED;
  } else {
    return NetworkChangeNotifier::CONNECTION_COST_METERED;
  }
}

NetworkCostChangeNotifierWin::CoCreateInstanceCallback&
GetCoCreateInstanceCallback() {
  static base::NoDestructor<
      NetworkCostChangeNotifierWin::CoCreateInstanceCallback>
      co_create_instance_callback{base::BindRepeating(&CoCreateInstance)};
  return *co_create_instance_callback;
}

}  // namespace

// This class is used as an event sink to register for notifications from the
// `INetworkCostManagerEvents` interface. In particular, we are focused on
// getting notified when the connection cost changes.
class NetworkCostManagerEventSinkWin final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          INetworkCostManagerEvents> {
 public:
  static HRESULT CreateInstance(
      INetworkCostManager* network_cost_manager,
      base::RepeatingClosure cost_changed_callback,
      ComPtr<NetworkCostManagerEventSinkWin>* result) {
    ComPtr<NetworkCostManagerEventSinkWin> instance =
        Microsoft::WRL::Make<net::NetworkCostManagerEventSinkWin>(
            cost_changed_callback);
    HRESULT hr = instance->RegisterForNotifications(network_cost_manager);
    if (hr != S_OK) {
      return hr;
    }

    *result = instance;
    return S_OK;
  }

  void UnRegisterForNotifications() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (event_sink_connection_point_) {
      event_sink_connection_point_->Unadvise(event_sink_connection_cookie_);
      event_sink_connection_point_.Reset();
    }
  }

  // Implement the INetworkCostManagerEvents interface.
  HRESULT __stdcall CostChanged(DWORD /*cost*/,
                                NLM_SOCKADDR* /*socket_address*/) final {
    // It is possible to get multiple notifications in a short period of time.
    // Rather than worrying about whether this notification represents the
    // latest, just notify the owner who can get the current value from the
    // INetworkCostManager so we know that we're actually getting the correct
    // value.
    cost_changed_callback_.Run();
    return S_OK;
  }

  HRESULT __stdcall DataPlanStatusChanged(
      NLM_SOCKADDR* /*socket_address*/) final {
    return S_OK;
  }

  NetworkCostManagerEventSinkWin(base::RepeatingClosure cost_changed_callback)
      : cost_changed_callback_(cost_changed_callback) {}

  NetworkCostManagerEventSinkWin(const NetworkCostManagerEventSinkWin&) =
      delete;
  NetworkCostManagerEventSinkWin& operator=(
      const NetworkCostManagerEventSinkWin&) = delete;

 private:
  ~NetworkCostManagerEventSinkWin() final = default;

  HRESULT RegisterForNotifications(INetworkCostManager* cost_manager) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::win::AssertComInitialized();
    base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

    ComPtr<IUnknown> this_event_sink_unknown;
    HRESULT hr = QueryInterface(IID_PPV_ARGS(&this_event_sink_unknown));

    // `NetworkCostManagerEventSinkWin::QueryInterface` for `IUnknown` must
    // succeed since it is implemented by this class.
    CHECK_EQ(hr, S_OK);

    ComPtr<IConnectionPointContainer> connection_point_container;
    hr =
        cost_manager->QueryInterface(IID_PPV_ARGS(&connection_point_container));
    if (hr != S_OK) {
      return hr;
    }

    Microsoft::WRL::ComPtr<IConnectionPoint> event_sink_connection_point;
    hr = connection_point_container->FindConnectionPoint(
        IID_INetworkCostManagerEvents, &event_sink_connection_point);
    if (hr != S_OK) {
      return hr;
    }

    hr = event_sink_connection_point->Advise(this_event_sink_unknown.Get(),
                                             &event_sink_connection_cookie_);
    if (hr != S_OK) {
      return hr;
    }

    CHECK_EQ(event_sink_connection_point_, nullptr);
    event_sink_connection_point_ = event_sink_connection_point;
    return S_OK;
  }

  base::RepeatingClosure cost_changed_callback_;

  // The following members must be accessed on the sequence from
  // `sequence_checker_`
  SEQUENCE_CHECKER(sequence_checker_);
  DWORD event_sink_connection_cookie_ = 0;
  Microsoft::WRL::ComPtr<IConnectionPoint> event_sink_connection_point_;
};

// static
base::SequenceBound<NetworkCostChangeNotifierWin>
NetworkCostChangeNotifierWin::CreateInstance(
    CostChangedCallback cost_changed_callback) {
  scoped_refptr<base::SequencedTaskRunner> com_best_effort_task_runner =
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  return base::SequenceBound<NetworkCostChangeNotifierWin>(
      com_best_effort_task_runner,
      // Ensure `cost_changed_callback` runs on the sequence of the creator and
      // owner of `NetworkCostChangeNotifierWin`.
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         cost_changed_callback));
}

NetworkCostChangeNotifierWin::NetworkCostChangeNotifierWin(
    CostChangedCallback cost_changed_callback)
    : cost_changed_callback_(cost_changed_callback) {
  StartWatching();
}

NetworkCostChangeNotifierWin::~NetworkCostChangeNotifierWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopWatching();
}

void NetworkCostChangeNotifierWin::StartWatching() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::win::GetVersion() < kSupportedOsVersion) {
    return;
  }

  base::win::AssertComInitialized();
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // Create `INetworkListManager` using `CoCreateInstance()`.  Tests may provide
  // a fake implementation of `INetworkListManager` through an
  // `OverrideCoCreateInstanceForTesting()`.
  ComPtr<INetworkCostManager> cost_manager;
  HRESULT hr = GetCoCreateInstanceCallback().Run(
      CLSID_NetworkListManager, /*unknown_outer=*/nullptr, CLSCTX_ALL,
      IID_INetworkCostManager, &cost_manager);
  if (hr != S_OK) {
    return;
  }

  // Subscribe to cost changed events.
  hr = NetworkCostManagerEventSinkWin::CreateInstance(
      cost_manager.Get(),
      // Cost changed callbacks must run on this sequence to get the new cost
      // from `INetworkCostManager`.
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&NetworkCostChangeNotifierWin::HandleCostChanged,
                              weak_ptr_factory_.GetWeakPtr())),
      &cost_manager_event_sink_);

  if (hr != S_OK) {
    return;
  }

  // Set the initial cost and inform observers of the initial value.
  cost_manager_ = cost_manager;
  HandleCostChanged();
}

void NetworkCostChangeNotifierWin::StopWatching() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cost_manager_event_sink_) {
    cost_manager_event_sink_->UnRegisterForNotifications();
    cost_manager_event_sink_.Reset();
  }

  cost_manager_.Reset();
}

void NetworkCostChangeNotifierWin::HandleCostChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DWORD connection_cost_flags;
  HRESULT hr = cost_manager_->GetCost(&connection_cost_flags,
                                      /*destination_ip_address=*/nullptr);
  if (hr != S_OK) {
    connection_cost_flags = NLM_CONNECTION_COST_UNKNOWN;
  }

  NetworkChangeNotifier::ConnectionCost changed_cost =
      ConnectionCostFromNlmConnectionCost(connection_cost_flags);

  cost_changed_callback_.Run(changed_cost);
}

// static
void NetworkCostChangeNotifierWin::OverrideCoCreateInstanceForTesting(
    CoCreateInstanceCallback callback_for_testing) {
  GetCoCreateInstanceCallback() = callback_for_testing;
}

}  // namespace net
