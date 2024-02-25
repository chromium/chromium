// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_service_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_observer.h"

using content::BrowserThread;
using wifi::WiFiService;

namespace extensions {

namespace {

// Deletes WiFiService object on the worker thread.
void ShutdownWifiServiceOnWorkerThread(
    std::unique_ptr<WiFiService> wifi_service) {
  DCHECK(wifi_service.get());
}

// Ensure that all calls to WiFiService are called from the same task runner
// since the implementations do not provide any thread safety gaurantees.
base::LazyThreadPoolSequencedTaskRunner g_sequenced_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

}  // namespace

NetworkingPrivateServiceClient::ServiceCallbacks::ServiceCallbacks() = default;

NetworkingPrivateServiceClient::ServiceCallbacks::~ServiceCallbacks() = default;

NetworkingPrivateServiceClient::NetworkingPrivateServiceClient(
    std::unique_ptr<WiFiService> wifi_service)
    : wifi_service_(std::move(wifi_service)),
      task_runner_(g_sequenced_task_runner.Get()) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WiFiService::Initialize,
                     base::Unretained(wifi_service_.get()), task_runner_));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WiFiService::SetEventObservers,
          base::Unretained(wifi_service_.get()),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(
              &NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&NetworkingPrivateServiceClient::
                                  OnNetworkListChangedEventOnUIThread,
                              weak_factory_.GetWeakPtr())));
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

NetworkingPrivateServiceClient::~NetworkingPrivateServiceClient() {
  // Verify that wifi_service was passed to ShutdownWifiServiceOnWorkerThread to
  // be deleted after completion of all posted tasks.
  DCHECK(!wifi_service_.get());
}

void NetworkingPrivateServiceClient::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  // Clear callbacks map to release callbacks from UI thread.
  callbacks_map_.Clear();
  // Post ShutdownWifiServiceOnWorkerThread task to delete services when all
  // posted tasks are done.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ShutdownWifiServiceOnWorkerThread,
                                        std::move(wifi_service_)));
}

void NetworkingPrivateServiceClient::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  network_events_observers_.AddObserver(observer);
}

void NetworkingPrivateServiceClient::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  network_events_observers_.RemoveObserver(observer);
}

void NetworkingPrivateServiceClient::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WiFiService::RequestConnectedNetworkUpdate,
                                base::Unretained(wifi_service_.get())));
}

NetworkingPrivateServiceClient::ServiceCallbacks*
NetworkingPrivateServiceClient::AddServiceCallbacks() {
  ServiceCallbacks* service_callbacks = new ServiceCallbacks();
  service_callbacks->id =
      callbacks_map_.Add(base::WrapUnique(service_callbacks));
  return service_callbacks;
}

void NetworkingPrivateServiceClient::RemoveServiceCallbacks(
    ServiceCallbacksID callback_id) {
  callbacks_map_.Remove(callback_id);
}

// NetworkingPrivateServiceClient implementation

void NetworkingPrivateServiceClient::GetProperties(
    const std::string& guid,
    PropertiesCallback callback) {
  auto properties = std::make_unique<base::Value::Dict>();
  std::string* error = new std::string;

  base::Value::Dict* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::GetProperties,
                     base::Unretained(wifi_service_.get()), guid,
                     properties_ptr, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterGetProperties,
                     weak_factory_.GetWeakPtr(), std::move(callback), guid,
                     std::move(properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetManagedProperties(
    const std::string& guid,
    PropertiesCallback callback) {
  auto properties = std::make_unique<base::Value::Dict>();
  std::string* error = new std::string;

  base::Value::Dict* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::GetManagedProperties,
                     base::Unretained(wifi_service_.get()), guid,
                     properties_ptr, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterGetProperties,
                     weak_factory_.GetWeakPtr(), std::move(callback), guid,
                     std::move(properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetState(
    const std::string& guid,
    DictionaryCallback success_callback,
    FailureCallback failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->get_properties_callback = std::move(success_callback);

  auto properties = std::make_unique<base::Value::Dict>();
  std::string* error = new std::string;

  base::Value::Dict* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::GetState,
                     base::Unretained(wifi_service_.get()), guid,
                     properties_ptr, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterGetState,
                     weak_factory_.GetWeakPtr(), service_callbacks->id, guid,
                     std::move(properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::SetProperties(
    const std::string& guid,
    base::Value::Dict properties,
    bool allow_set_shared_config,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  CHECK(allow_set_shared_config);

  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->set_properties_callback = std::move(success_callback);

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::SetProperties,
                     base::Unretained(wifi_service_.get()), guid,
                     std::move(properties), error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterSetProperties,
                     weak_factory_.GetWeakPtr(), service_callbacks->id,
                     base::Owned(error)));
}

void NetworkingPrivateServiceClient::CreateNetwork(
    bool shared,
    base::Value::Dict properties,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->create_network_callback = std::move(success_callback);

  std::string* network_guid = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::CreateNetwork,
                     base::Unretained(wifi_service_.get()), shared,
                     std::move(properties), network_guid, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterCreateNetwork,
                     weak_factory_.GetWeakPtr(), service_callbacks->id,
                     base::Owned(network_guid), base::Owned(error)));
}

void NetworkingPrivateServiceClient::ForgetNetwork(
    const std::string& guid,
    bool allow_forget_shared_config,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  // TODO(mef): Implement for Win/Mac
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    NetworkListCallback success_callback,
    FailureCallback failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->get_visible_networks_callback =
      std::move(success_callback);

  auto networks = std::make_unique<base::Value::List>();

  // TODO(stevenjb/mef): Apply filters (configured, visible, limit).

  base::Value::List* networks_ptr = networks.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::GetVisibleNetworks,
                     base::Unretained(wifi_service_.get()), network_type,
                     /*include_details=*/false, networks_ptr),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterGetVisibleNetworks,
                     weak_factory_.GetWeakPtr(), service_callbacks->id,
                     std::move(networks)));
}

void NetworkingPrivateServiceClient::StartConnect(
    const std::string& guid,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->start_connect_callback = std::move(success_callback);

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::StartConnect,
                     base::Unretained(wifi_service_.get()), guid, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterStartConnect,
                     weak_factory_.GetWeakPtr(), service_callbacks->id,
                     base::Owned(error)));
}

void NetworkingPrivateServiceClient::StartDisconnect(
    const std::string& guid,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = std::move(failure_callback);
  service_callbacks->start_disconnect_callback = std::move(success_callback);

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WiFiService::StartDisconnect,
                     base::Unretained(wifi_service_.get()), guid, error),
      base::BindOnce(&NetworkingPrivateServiceClient::AfterStartDisconnect,
                     weak_factory_.GetWeakPtr(), service_callbacks->id,
                     base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetCaptivePortalStatus(
    const std::string& guid,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetEnabledNetworkTypes(
    EnabledNetworkTypesCallback callback) {
  base::Value::List network_list;
  network_list.Append(::onc::network_type::kWiFi);
  std::move(callback).Run(std::move(network_list));
}

void NetworkingPrivateServiceClient::GetDeviceStateList(
    DeviceStateListCallback callback) {
  DeviceStateList device_state_list;
  api::networking_private::DeviceStateProperties& properties =
      device_state_list.emplace_back();
  properties.type = api::networking_private::NetworkType::kWiFi;
  properties.state = api::networking_private::DeviceStateType::kEnabled;
  std::move(callback).Run(std::move(device_state_list));
}

void NetworkingPrivateServiceClient::GetGlobalPolicy(
    GetGlobalPolicyCallback callback) {
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateServiceClient::GetCertificateLists(
    GetCertificateListsCallback callback) {
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateServiceClient::EnableNetworkType(const std::string& type,
                                                       BoolCallback callback) {
  std::move(callback).Run(false);
}

void NetworkingPrivateServiceClient::DisableNetworkType(const std::string& type,
                                                        BoolCallback callback) {
  std::move(callback).Run(false);
}

void NetworkingPrivateServiceClient::RequestScan(const std::string& /* type */,
                                                 BoolCallback callback) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WiFiService::RequestNetworkScan,
                                        base::Unretained(wifi_service_.get())));
  std::move(callback).Run(true);
}

////////////////////////////////////////////////////////////////////////////////

void NetworkingPrivateServiceClient::AfterGetProperties(
    PropertiesCallback callback,
    const std::string& network_guid,
    std::unique_ptr<base::Value::Dict> properties,
    const std::string* error) {
  if (!error->empty()) {
    std::move(callback).Run(std::nullopt, *error);
    return;
  }
  std::move(callback).Run(std::move(*properties), std::nullopt);
}

void NetworkingPrivateServiceClient::AfterGetState(
    ServiceCallbacksID callback_id,
    const std::string& network_guid,
    std::unique_ptr<base::Value::Dict> properties,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    std::move(service_callbacks->failure_callback).Run(*error);
  } else {
    DCHECK(!service_callbacks->get_properties_callback.is_null());
    std::move(service_callbacks->get_properties_callback)
        .Run(std::move(*properties));
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterGetVisibleNetworks(
    ServiceCallbacksID callback_id,
    std::unique_ptr<base::Value::List> networks) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  DCHECK(!service_callbacks->get_visible_networks_callback.is_null());
  std::move(service_callbacks->get_visible_networks_callback)
      .Run(std::move(*networks));
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterSetProperties(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    std::move(service_callbacks->failure_callback).Run(*error);
  } else {
    DCHECK(!service_callbacks->set_properties_callback.is_null());
    std::move(service_callbacks->set_properties_callback).Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterCreateNetwork(
    ServiceCallbacksID callback_id,
    const std::string* network_guid,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    std::move(service_callbacks->failure_callback).Run(*error);
  } else {
    DCHECK(!service_callbacks->create_network_callback.is_null());
    std::move(service_callbacks->create_network_callback).Run(*network_guid);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterStartConnect(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    std::move(service_callbacks->failure_callback).Run(*error);
  } else {
    DCHECK(!service_callbacks->start_connect_callback.is_null());
    std::move(service_callbacks->start_connect_callback).Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterStartDisconnect(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    std::move(service_callbacks->failure_callback).Run(*error);
  } else {
    DCHECK(!service_callbacks->start_disconnect_callback.is_null());
    std::move(service_callbacks->start_disconnect_callback).Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : network_events_observers_) {
    observer.OnNetworksChangedEvent(network_guids);
  }
}

void NetworkingPrivateServiceClient::OnNetworkListChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : network_events_observers_) {
    observer.OnNetworkListChangedEvent(network_guids);
  }
}

}  // namespace extensions
