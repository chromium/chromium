// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/win/fake_network_cost_manager.h"

#include <netlistmgr.h>
#include <wrl/implements.h>

#include <map>

#include "base/task/sequenced_task_runner.h"
#include "net/base/network_cost_change_notifier_win.h"

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

namespace net {

namespace {

DWORD NlmConnectionCostFlagsFromConnectionCost(
    NetworkChangeNotifier::ConnectionCost source_cost) {
  switch (source_cost) {
    case NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED:
      return (NLM_CONNECTION_COST_UNRESTRICTED | NLM_CONNECTION_COST_CONGESTED);
    case NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED:
      return (NLM_CONNECTION_COST_VARIABLE | NLM_CONNECTION_COST_ROAMING |
              NLM_CONNECTION_COST_APPROACHINGDATALIMIT);
    case NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN:
    default:
      return NLM_CONNECTION_COST_UNKNOWN;
  }
}

void DispatchCostChangedEvent(ComPtr<INetworkCostManagerEvents> event_target,
                              DWORD cost) {
  std::ignore =
      event_target->CostChanged(cost, /*destination_address=*/nullptr);
}

}  // namespace

// A fake implementation of `INetworkCostManager` that can simulate costs,
// changed costs and errors.
class FakeNetworkCostManager final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          INetworkCostManager,
                          IConnectionPointContainer,
                          IConnectionPoint> {
 public:
  FakeNetworkCostManager(NetworkChangeNotifier::ConnectionCost connection_cost,
                         NetworkCostManagerStatus error_status)
      : error_status_(error_status), connection_cost_(connection_cost) {}

  // For each event sink in `event_sinks_`, call
  // `INetworkCostManagerEvents::CostChanged()` with `changed_cost` on the event
  // sink's task runner.
  void PostCostChangedEvents(
      NetworkChangeNotifier::ConnectionCost changed_cost) {
    DWORD cost_for_changed_event;
    std::map</*event_sink_cookie=*/DWORD, EventSinkRegistration>
        event_sinks_for_changed_event;
    {
      base::AutoLock auto_lock(member_lock_);
      connection_cost_ = changed_cost;
      cost_for_changed_event =
          NlmConnectionCostFlagsFromConnectionCost(changed_cost);

      // Get the snapshot of event sinks to notify.  The snapshot collection
      // creates a new `ComPtr` for each event sink, which increments each the
      // event sink's reference count, ensuring that each event sink
      // remains alive to receive the cost changed event notification.
      event_sinks_for_changed_event = event_sinks_;
    }

    for (const auto& pair : event_sinks_for_changed_event) {
      const auto& registration = pair.second;
      registration.event_sink_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DispatchCostChangedEvent, registration.event_sink_,
                         cost_for_changed_event));
    }
  }

  // Implement the `INetworkCostManager` interface.
  HRESULT
  __stdcall GetCost(DWORD* cost,
                    NLM_SOCKADDR* destination_ip_address) override {
    if (error_status_ == NetworkCostManagerStatus::kErrorGetCostFailed) {
      return E_FAIL;
    }

    if (destination_ip_address != nullptr) {
      NOTIMPLEMENTED();
      return E_NOTIMPL;
    }

    {
      base::AutoLock auto_lock(member_lock_);
      *cost = NlmConnectionCostFlagsFromConnectionCost(connection_cost_);
    }
    return S_OK;
  }

  HRESULT __stdcall GetDataPlanStatus(
      NLM_DATAPLAN_STATUS* data_plan_status,
      NLM_SOCKADDR* destination_ip_address) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  HRESULT __stdcall SetDestinationAddresses(
      UINT32 length,
      NLM_SOCKADDR* destination_ip_address_list,
      VARIANT_BOOL append) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  // Implement the `IConnectionPointContainer` interface.
  HRESULT __stdcall FindConnectionPoint(REFIID connection_point_id,
                                        IConnectionPoint** result) override {
    if (error_status_ ==
        NetworkCostManagerStatus::kErrorFindConnectionPointFailed) {
      return E_ABORT;
    }

    if (connection_point_id != IID_INetworkCostManagerEvents) {
      return E_NOINTERFACE;
    }

    *result = static_cast<IConnectionPoint*>(this);
    AddRef();
    return S_OK;
  }

  HRESULT __stdcall EnumConnectionPoints(
      IEnumConnectionPoints** results) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  // Implement the `IConnectionPoint` interface.
  HRESULT __stdcall Advise(IUnknown* event_sink,
                           DWORD* event_sink_cookie) override {
    if (error_status_ == NetworkCostManagerStatus::kErrorAdviseFailed) {
      return E_NOT_VALID_STATE;
    }

    ComPtr<INetworkCostManagerEvents> cost_manager_event_sink;
    HRESULT hr =
        event_sink->QueryInterface(IID_PPV_ARGS(&cost_manager_event_sink));
    if (hr != S_OK) {
      return hr;
    }

    base::AutoLock auto_lock(member_lock_);

    event_sinks_[next_event_sink_cookie_] = {
        cost_manager_event_sink,
        base::SequencedTaskRunner::GetCurrentDefault()};

    *event_sink_cookie = next_event_sink_cookie_;
    ++next_event_sink_cookie_;

    return S_OK;
  }

  HRESULT __stdcall Unadvise(DWORD event_sink_cookie) override {
    base::AutoLock auto_lock(member_lock_);

    auto it = event_sinks_.find(event_sink_cookie);
    if (it == event_sinks_.end()) {
      return ERROR_NOT_FOUND;
    }

    event_sinks_.erase(it);
    return S_OK;
  }

  HRESULT __stdcall GetConnectionInterface(IID* result) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  HRESULT __stdcall GetConnectionPointContainer(
      IConnectionPointContainer** result) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  HRESULT __stdcall EnumConnections(IEnumConnections** result) override {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  // Implement the `IUnknown` interface.
  HRESULT __stdcall QueryInterface(REFIID interface_id,
                                   void** result) override {
    if (error_status_ == NetworkCostManagerStatus::kErrorQueryInterfaceFailed) {
      return E_NOINTERFACE;
    }
    return RuntimeClass<RuntimeClassFlags<ClassicCom>, INetworkCostManager,
                        IConnectionPointContainer,
                        IConnectionPoint>::QueryInterface(interface_id, result);
  }

  FakeNetworkCostManager(const FakeNetworkCostManager&) = delete;
  FakeNetworkCostManager& operator=(const FakeNetworkCostManager&) = delete;

 private:
  // The error state for this `FakeNetworkCostManager` to simulate.  Cannot be
  // changed.
  const NetworkCostManagerStatus error_status_;

  // Synchronizes access to all members below.
  base::Lock member_lock_;

  NetworkChangeNotifier::ConnectionCost connection_cost_
      GUARDED_BY(member_lock_);

  DWORD next_event_sink_cookie_ GUARDED_BY(member_lock_) = 0;

  struct EventSinkRegistration {
    ComPtr<INetworkCostManagerEvents> event_sink_;
    scoped_refptr<base::SequencedTaskRunner> event_sink_task_runner_;
  };
  std::map</*event_sink_cookie=*/DWORD, EventSinkRegistration> event_sinks_
      GUARDED_BY(member_lock_);
};

FakeNetworkCostManagerEnvironment::FakeNetworkCostManagerEnvironment() {
  // Set up `NetworkCostChangeNotifierWin` to use the fake OS APIs.
  NetworkCostChangeNotifierWin::OverrideCoCreateInstanceForTesting(
      base::BindRepeating(
          &FakeNetworkCostManagerEnvironment::FakeCoCreateInstance,
          base::Unretained(this)));
}

FakeNetworkCostManagerEnvironment::~FakeNetworkCostManagerEnvironment() {
  // Restore `NetworkCostChangeNotifierWin` to use the real OS APIs.
  NetworkCostChangeNotifierWin::OverrideCoCreateInstanceForTesting(
      base::BindRepeating(&CoCreateInstance));
}

HRESULT FakeNetworkCostManagerEnvironment::FakeCoCreateInstance(
    REFCLSID class_id,
    LPUNKNOWN outer_aggregate,
    DWORD context_flags,
    REFIID interface_id,
    LPVOID* result) {
  NetworkChangeNotifier::ConnectionCost connection_cost_for_new_instance;
  NetworkCostManagerStatus error_status_for_new_instance;
  {
    base::AutoLock auto_lock(member_lock_);
    connection_cost_for_new_instance = connection_cost_;
    error_status_for_new_instance = error_status_;
  }

  if (error_status_for_new_instance ==
      NetworkCostManagerStatus::kErrorCoCreateInstanceFailed) {
    return E_ACCESSDENIED;
  }

  if (class_id != CLSID_NetworkListManager) {
    return E_NOINTERFACE;
  }

  if (interface_id != IID_INetworkCostManager) {
    return E_NOINTERFACE;
  }

  ComPtr<FakeNetworkCostManager> instance =
      Microsoft::WRL::Make<FakeNetworkCostManager>(
          connection_cost_for_new_instance, error_status_for_new_instance);
  {
    base::AutoLock auto_lock(member_lock_);
    fake_network_cost_managers_.push_back(instance);
  }
  *result = instance.Detach();
  return S_OK;
}

void FakeNetworkCostManagerEnvironment::SetCost(
    NetworkChangeNotifier::ConnectionCost value) {
  // Update the cost for each `INetworkCostManager` instance in
  // `fake_network_cost_managers_`.
  std::vector<Microsoft::WRL::ComPtr<FakeNetworkCostManager>>
      fake_network_cost_managers_for_change_event;
  {
    base::AutoLock auto_lock(member_lock_);
    connection_cost_ = value;
    fake_network_cost_managers_for_change_event = fake_network_cost_managers_;
  }

  for (const auto& network_cost_manager :
       fake_network_cost_managers_for_change_event) {
    network_cost_manager->PostCostChangedEvents(/*connection_cost=*/value);
  }
}

void FakeNetworkCostManagerEnvironment::SimulateError(
    NetworkCostManagerStatus error_status) {
  base::AutoLock auto_lock(member_lock_);
  error_status_ = error_status;
}

}  // namespace net
