// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_service_client.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
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
base::LazySequencedTaskRunner g_sequenced_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(base::TaskTraits(
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

}  // namespace

NetworkingPrivateServiceClient::ServiceCallbacks::ServiceCallbacks() {
}

NetworkingPrivateServiceClient::ServiceCallbacks::~ServiceCallbacks() {
}

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
          base::ThreadTaskRunnerHandle::Get(),
          base::Bind(
              &NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread,
              weak_factory_.GetWeakPtr()),
          base::Bind(&NetworkingPrivateServiceClient::
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
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::GetProperties,
                            base::Unretained(wifi_service_.get()), guid,
                            properties_ptr, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(), service_callbacks->id, guid,
                 base::Passed(&properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetManagedProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::GetManagedProperties,
                            base::Unretained(wifi_service_.get()), guid,
                            properties_ptr, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(), service_callbacks->id, guid,
                 base::Passed(&properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetState(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetState, base::Unretained(wifi_service_.get()),
                 guid, properties_ptr, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(), service_callbacks->id, guid,
                 base::Passed(&properties), base::Owned(error)));
}

void NetworkingPrivateServiceClient::SetProperties(
    const std::string& guid,
    std::unique_ptr<base::DictionaryValue> properties,
    bool allow_set_shared_config,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  CHECK(allow_set_shared_config);

  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->set_properties_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::SetProperties,
                            base::Unretained(wifi_service_.get()), guid,
                            base::Passed(&properties), error),
      base::Bind(&NetworkingPrivateServiceClient::AfterSetProperties,
                 weak_factory_.GetWeakPtr(), service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::CreateNetwork(
    bool shared,
    std::unique_ptr<base::DictionaryValue> properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->create_network_callback = success_callback;

  std::string* network_guid = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::CreateNetwork,
                            base::Unretained(wifi_service_.get()), shared,
                            base::Passed(&properties), network_guid, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterCreateNetwork,
                 weak_factory_.GetWeakPtr(), service_callbacks->id,
                 base::Owned(network_guid), base::Owned(error)));
}

void NetworkingPrivateServiceClient::ForgetNetwork(
    const std::string& guid,
    bool allow_forget_shared_config,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  // TODO(mef): Implement for Win/Mac
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    const NetworkListCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_visible_networks_callback = success_callback;

  std::unique_ptr<base::ListValue> networks(new base::ListValue);

  // TODO(stevenjb/mef): Apply filters (configured, visible, limit).

  base::ListValue* networks_ptr = networks.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::GetVisibleNetworks,
                            base::Unretained(wifi_service_.get()), network_type,
                            networks_ptr, false),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetVisibleNetworks,
                 weak_factory_.GetWeakPtr(), service_callbacks->id,
                 base::Passed(&networks)));
}

void NetworkingPrivateServiceClient::StartConnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->start_connect_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::StartConnect,
                            base::Unretained(wifi_service_.get()), guid, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartConnect,
                 weak_factory_.GetWeakPtr(), service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::StartDisconnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->start_disconnect_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&WiFiService::StartDisconnect,
                            base::Unretained(wifi_service_.get()), guid, error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartDisconnect,
                 weak_factory_.GetWeakPtr(), service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::SetWifiTDLSEnabledState(
    const std::string& ip_or_mac_address,
    bool enabled,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetCaptivePortalStatus(
    const std::string& guid,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

std::unique_ptr<base::ListValue>
NetworkingPrivateServiceClient::GetEnabledNetworkTypes() {
  auto network_list = std::make_unique<base::ListValue>();
  network_list->AppendString(::onc::network_type::kWiFi);
  return network_list;
}

std::unique_ptr<NetworkingPrivateDelegate::DeviceStateList>
NetworkingPrivateServiceClient::GetDeviceStateList() {
  std::unique_ptr<DeviceStateList> device_state_list(new DeviceStateList);
  std::unique_ptr<api::networking_private::DeviceStateProperties> properties(
      new api::networking_private::DeviceStateProperties);
  properties->type = api::networking_private::NETWORK_TYPE_WIFI;
  properties->state = api::networking_private::DEVICE_STATE_TYPE_ENABLED;
  device_state_list->push_back(std::move(properties));
  return device_state_list;
}

std::unique_ptr<base::DictionaryValue>
NetworkingPrivateServiceClient::GetGlobalPolicy() {
  return std::make_unique<base::DictionaryValue>();
}

std::unique_ptr<base::DictionaryValue>
NetworkingPrivateServiceClient::GetCertificateLists() {
  return std::make_unique<base::DictionaryValue>();
}

bool NetworkingPrivateServiceClient::EnableNetworkType(
    const std::string& type) {
  return false;
}

bool NetworkingPrivateServiceClient::DisableNetworkType(
    const std::string& type) {
  return false;
}

bool NetworkingPrivateServiceClient::RequestScan(
    const std::string& /* type */) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WiFiService::RequestNetworkScan,
                                        base::Unretained(wifi_service_.get())));
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void NetworkingPrivateServiceClient::AfterGetProperties(
    ServiceCallbacksID callback_id,
    const std::string& network_guid,
    std::unique_ptr<base::DictionaryValue> properties,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->get_properties_callback.is_null());
    service_callbacks->get_properties_callback.Run(std::move(properties));
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterGetVisibleNetworks(
    ServiceCallbacksID callback_id,
    std::unique_ptr<base::ListValue> networks) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  DCHECK(!service_callbacks->get_visible_networks_callback.is_null());
  service_callbacks->get_visible_networks_callback.Run(std::move(networks));
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterSetProperties(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->set_properties_callback.is_null());
    service_callbacks->set_properties_callback.Run();
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
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->create_network_callback.is_null());
    service_callbacks->create_network_callback.Run(*network_guid);
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
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->start_connect_callback.is_null());
    service_callbacks->start_connect_callback.Run();
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
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->start_disconnect_callback.is_null());
    service_callbacks->start_disconnect_callback.Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : network_events_observers_)
    observer.OnNetworksChangedEvent(network_guids);
}

void NetworkingPrivateServiceClient::OnNetworkListChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : network_events_observers_)
    observer.OnNetworkListChangedEvent(network_guids);
}

}  // namespace extensions
