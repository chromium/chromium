// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_linux.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "extensions/browser/api/networking_private/network_config_dbus_constants_linux.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_observer.h"

////////////////////////////////////////////////////////////////////////////////

namespace extensions {

namespace {
// Access Point info strings.
const char kAccessPointInfoName[] = "Name";
const char kAccessPointInfoGuid[] = "GUID";
const char kAccessPointInfoConnectable[] = "Connectable";
const char kAccessPointInfoConnectionState[] = "ConnectionState";
const char kAccessPointInfoType[] = "Type";
const char kAccessPointInfoTypeWifi[] = "WiFi";
const char kAccessPointInfoWifiSignalStrengthDotted[] = "WiFi.SignalStrength";
const char kAccessPointInfoWifiSecurityDotted[] = "WiFi.Security";

// Access point security type strings.
const char kAccessPointSecurityNone[] = "None";
const char kAccessPointSecurityUnknown[] = "Unknown";
const char kAccessPointSecurityWpaPsk[] = "WPA-PSK";
const char kAccessPointSecurity9021X[] = "WEP-8021X";

// Parses the GUID which contains 3 pieces of relevant information. The
// object path to the network device, the object path of the access point,
// and the ssid.
bool ParseNetworkGuid(const std::string& guid,
                      std::string* device_path,
                      std::string* access_point_path,
                      std::string* ssid) {
  std::vector<std::string> guid_parts =
      base::SplitString(guid, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (guid_parts.size() != 3) {
    return false;
  }

  *device_path = guid_parts[0];
  *access_point_path = guid_parts[1];
  *ssid = guid_parts[2];

  if (device_path->empty() || access_point_path->empty() || ssid->empty()) {
    return false;
  }

  return true;
}

// Simplified helper to parse the SSID from the GUID.
bool GuidToSsid(const std::string& guid, std::string* ssid) {
  std::string unused_1;
  std::string unused_2;
  return ParseNetworkGuid(guid, &unused_1, &unused_2, ssid);
}

// Iterates over the map cloning the contained networks to a
// list then returns the list.
base::Value::List CopyNetworkMapToList(
    const NetworkingPrivateLinux::NetworkMap& network_map) {
  base::Value::List network_list;

  for (const auto& network : network_map) {
    network_list.Append(network.second.Clone());
  }

  return network_list;
}

// Constructs a network guid from its constituent parts.
std::string ConstructNetworkGuid(const dbus::ObjectPath& device_path,
                                 const dbus::ObjectPath& access_point_path,
                                 const std::string& ssid) {
  return device_path.value() + "|" + access_point_path.value() + "|" + ssid;
}

// Logs that the method is not implemented and reports |kErrorNotSupported|
// to the failure callback.
void ReportNotSupported(
    const std::string& method_name,
    NetworkingPrivateDelegate::FailureCallback failure_callback) {
  LOG(WARNING) << method_name << " is not supported";
  std::move(failure_callback)
      .Run(extensions::networking_private::kErrorNotSupported);
}

// Fires the appropriate callback when the network connect operation succeeds
// or fails.
void OnNetworkConnectOperationCompleted(
    std::unique_ptr<std::string> error,
    NetworkingPrivateDelegate::VoidCallback success_callback,
    NetworkingPrivateDelegate::FailureCallback failure_callback) {
  if (!error->empty()) {
    std::move(failure_callback).Run(*error);
    return;
  }
  std::move(success_callback).Run();
}

// Fires the appropriate callback when the network properties are returned
// from the |dbus_thread_|.
void GetCachedNetworkPropertiesCallback(
    std::unique_ptr<std::string> error,
    std::unique_ptr<base::Value::Dict> properties,
    NetworkingPrivateDelegate::DictionaryCallback success_callback,
    NetworkingPrivateDelegate::FailureCallback failure_callback) {
  if (!error->empty()) {
    std::move(failure_callback).Run(*error);
    return;
  }
  std::move(success_callback).Run(std::move(*properties));
}

// Fires the appropriate callback when the network properties are returned
// from the |dbus_thread_|.
void GetCachedNetworkPropertiesResultCallback(
    std::unique_ptr<std::string> error,
    std::unique_ptr<base::Value::Dict> properties,
    NetworkingPrivateDelegate::PropertiesCallback callback) {
  if (!error->empty()) {
    LOG(ERROR) << "GetCachedNetworkProperties failed: " << *error;
    std::move(callback).Run(std::nullopt, *error);
    return;
  }
  std::move(callback).Run(std::move(*properties), std::nullopt);
}

}  // namespace

NetworkingPrivateLinux::NetworkingPrivateLinux()
    : dbus_thread_("Networking Private DBus"), network_manager_proxy_(nullptr) {
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);

  dbus_thread_.StartWithOptions(std::move(thread_options));
  dbus_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&NetworkingPrivateLinux::Initialize,
                                base::Unretained(this)));
}

NetworkingPrivateLinux::~NetworkingPrivateLinux() {
  if (dbus_) {
    // dbus_thread_.Stop() below will wait for this task.
    dbus_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, dbus_));
  }
  dbus_thread_.Stop();
}

void NetworkingPrivateLinux::AssertOnDBusThread() {
  DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
}

void NetworkingPrivateLinux::Initialize() {
  dbus_task_runner_ = dbus_thread_.task_runner();
  // This has to be called after the task runner is initialized.
  AssertOnDBusThread();

  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  dbus_options.connection_type = dbus::Bus::PRIVATE;
  dbus_options.dbus_task_runner = dbus_task_runner_;

  dbus_ = base::MakeRefCounted<dbus::Bus>(dbus_options);
  network_manager_proxy_ = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace,
      dbus::ObjectPath(networking_private::kNetworkManagerPath));

  if (!network_manager_proxy_) {
    LOG(ERROR) << "Platform does not support NetworkManager over DBUS";
  }

  network_map_ = std::make_unique<NetworkMap>();
}

bool NetworkingPrivateLinux::CheckNetworkManagerSupported() {
  return network_manager_proxy_ != nullptr;
}

void NetworkingPrivateLinux::GetProperties(const std::string& guid,
                                           PropertiesCallback callback) {
  if (!network_manager_proxy_) {
    LOG(WARNING) << "NetworkManager over DBus is not supported";
    std::move(callback).Run(std::nullopt,
                            extensions::networking_private::kErrorNotSupported);
    return;
  }

  auto error = std::make_unique<std::string>();
  auto network_properties = std::make_unique<base::Value::Dict>();

  // Runs GetCachedNetworkProperties() on |dbus_thread|. We can safely pass the
  // internal raw pointers since it is guaranteed to outlive
  // GetCachedNetworkProperties() because ownership is given to the callback.
  std::string* error_ptr = error.get();
  base::Value::Dict* network_properties_ptr = network_properties.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::GetCachedNetworkProperties,
                     base::Unretained(this), guid,
                     base::Unretained(network_properties_ptr),
                     base::Unretained(error_ptr)),
      base::BindOnce(&GetCachedNetworkPropertiesResultCallback,
                     std::move(error), std::move(network_properties),
                     std::move(callback)));
}

void NetworkingPrivateLinux::GetManagedProperties(const std::string& guid,
                                                  PropertiesCallback callback) {
  LOG(WARNING) << "GetManagedProperties is not supported";
  std::move(callback).Run(std::nullopt,
                          extensions::networking_private::kErrorNotSupported);
}

void NetworkingPrivateLinux::GetState(const std::string& guid,
                                      DictionaryCallback success_callback,
                                      FailureCallback failure_callback) {
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("GetState", std::move(failure_callback));
    return;
  }

  auto error = std::make_unique<std::string>();
  auto network_properties = std::make_unique<base::Value::Dict>();

  // Runs GetCachedNetworkProperties() on |dbus_thread|. We can safely pass the
  // internal raw pointers since it is guaranteed to outlive
  // GetCachedNetworkProperties() because ownership is given to the callback.
  std::string* error_ptr = error.get();
  base::Value::Dict* network_properties_ptr = network_properties.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::GetCachedNetworkProperties,
                     base::Unretained(this), guid,
                     base::Unretained(network_properties_ptr),
                     base::Unretained(error_ptr)),
      base::BindOnce(&GetCachedNetworkPropertiesCallback, std::move(error),
                     std::move(network_properties), std::move(success_callback),
                     std::move(failure_callback)));
}

void NetworkingPrivateLinux::GetCachedNetworkProperties(
    const std::string& guid,
    base::Value::Dict* properties,
    std::string* error) {
  AssertOnDBusThread();
  std::string ssid;

  if (!GuidToSsid(guid, &ssid)) {
    *error = "Invalid Network GUID format";
    return;
  }

  NetworkMap::const_iterator network_iter =
      network_map_->find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map_->end()) {
    *error = "Unknown network GUID";
    return;
  }

  *properties = network_iter->second.Clone();
}

void NetworkingPrivateLinux::SetProperties(const std::string& guid,
                                           base::Value::Dict properties,
                                           bool allow_set_shared_config,
                                           VoidCallback success_callback,
                                           FailureCallback failure_callback) {
  ReportNotSupported("SetProperties", std::move(failure_callback));
}

void NetworkingPrivateLinux::CreateNetwork(bool shared,
                                           base::Value::Dict properties,
                                           StringCallback success_callback,
                                           FailureCallback failure_callback) {
  ReportNotSupported("CreateNetwork", std::move(failure_callback));
}

void NetworkingPrivateLinux::ForgetNetwork(const std::string& guid,
                                           bool allow_forget_shared_config,
                                           VoidCallback success_callback,
                                           FailureCallback failure_callback) {
  // TODO(zentaro): Implement for Linux.
  ReportNotSupported("ForgetNetwork", std::move(failure_callback));
}

void NetworkingPrivateLinux::GetNetworks(const std::string& network_type,
                                         bool configured_only,
                                         bool visible_only,
                                         int limit,
                                         NetworkListCallback success_callback,
                                         FailureCallback failure_callback) {
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("GetNetworks", std::move(failure_callback));
    return;
  }

  auto network_map = std::make_unique<NetworkMap>();

  if (!(network_type == ::onc::network_type::kWiFi ||
        network_type == ::onc::network_type::kWireless ||
        network_type == ::onc::network_type::kAllTypes)) {
    // Only enumerating WiFi networks is supported on linux.
    ReportNotSupported("GetNetworks with network_type=" + network_type,
                       std::move(failure_callback));
    return;
  }

  // Runs GetAllWiFiAccessPoints on the dbus_thread and returns the
  // results back to OnAccessPointsFound where the callback is fired.
  NetworkMap* network_map_ptr = network_map.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::GetAllWiFiAccessPoints,
                     base::Unretained(this), configured_only, visible_only,
                     limit, base::Unretained(network_map_ptr)),
      base::BindOnce(&NetworkingPrivateLinux::OnAccessPointsFound,
                     base::Unretained(this), std::move(network_map),
                     std::move(success_callback), std::move(failure_callback)));
}

bool NetworkingPrivateLinux::GetNetworksForScanRequest() {
  if (!network_manager_proxy_) {
    return false;
  }

  auto network_map = std::make_unique<NetworkMap>();

  // Runs GetAllWiFiAccessPoints on the dbus_thread and returns the
  // results back to SendNetworkListChangedEvent to fire the event. No
  // callbacks are used in this case.
  NetworkMap* network_map_ptr = network_map.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::GetAllWiFiAccessPoints,
                     base::Unretained(this), false /* configured_only */,
                     false /* visible_only */, 0 /* limit */,
                     base::Unretained(network_map_ptr)),
      base::BindOnce(&NetworkingPrivateLinux::OnAccessPointsFoundViaScan,
                     base::Unretained(this), std::move(network_map)));

  return true;
}

// Constructs the network configuration message and connects to the network.
// The message is of the form:
// {
//   '802-11-wireless': {
//     'ssid': 'FooNetwork'
//   }
// }
void NetworkingPrivateLinux::ConnectToNetwork(const std::string& guid,
                                              std::string* error) {
  AssertOnDBusThread();
  std::string device_path_str;
  std::string access_point_path_str;
  std::string ssid;
  DVLOG(1) << "Connecting to network GUID " << guid;

  if (!ParseNetworkGuid(guid, &device_path_str, &access_point_path_str,
                        &ssid)) {
    *error = "Invalid Network GUID format";
    return;
  }

  // Set the connection state to connecting in the map.
  if (!SetConnectionStateAndPostEvent(guid, ssid,
                                      ::onc::connection_state::kConnecting)) {
    *error = "Unknown network GUID";
    return;
  }

  dbus::ObjectPath device_path(device_path_str);
  dbus::ObjectPath access_point_path(access_point_path_str);

  dbus::MethodCall method_call(
      networking_private::kNetworkManagerNamespace,
      networking_private::kNetworkManagerAddAndActivateConnectionMethod);
  dbus::MessageWriter builder(&method_call);

  // Build up the settings nested dictionary.
  dbus::MessageWriter array_writer(&method_call);
  builder.OpenArray("{sa{sv}}", &array_writer);

  dbus::MessageWriter dict_writer(&method_call);
  array_writer.OpenDictEntry(&dict_writer);
  // TODO(zentaro): Support other network types. Currently only WiFi is
  // supported.
  dict_writer.AppendString(
      networking_private::kNetworkManagerConnectionConfig80211Wireless);

  dbus::MessageWriter wifi_array(&method_call);
  dict_writer.OpenArray("{sv}", &wifi_array);

  dbus::MessageWriter wifi_dict_writer(&method_call);
  wifi_array.OpenDictEntry(&wifi_dict_writer);
  wifi_dict_writer.AppendString(
      networking_private::kNetworkManagerConnectionConfigSsid);

  dbus::MessageWriter variant_writer(&method_call);
  wifi_dict_writer.OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(base::as_byte_span(ssid));

  // Close all the arrays and dicts.
  wifi_dict_writer.CloseContainer(&variant_writer);
  wifi_array.CloseContainer(&wifi_dict_writer);
  dict_writer.CloseContainer(&wifi_array);
  array_writer.CloseContainer(&dict_writer);
  builder.CloseContainer(&array_writer);

  builder.AppendObjectPath(device_path);
  builder.AppendObjectPath(access_point_path);

  std::unique_ptr<dbus::Response> response(
      network_manager_proxy_
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));
  if (!response) {
    LOG(ERROR) << "Failed to add a new connection";
    *error = "Failed to connect.";

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    return;
  }

  dbus::MessageReader reader(response.get());
  dbus::ObjectPath connection_settings_path;
  dbus::ObjectPath active_connection_path;

  if (!reader.PopObjectPath(&connection_settings_path)) {
    LOG(ERROR) << "Unexpected response for add connection path "
               << ": " << response->ToString();
    *error = "Failed to connect.";

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    return;
  }

  if (!reader.PopObjectPath(&active_connection_path)) {
    LOG(ERROR) << "Unexpected response for connection path "
               << ": " << response->ToString();
    *error = "Failed to connect.";

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    return;
  }

  // Set the connection state to Connected in the map.
  SetConnectionStateAndPostEvent(guid, ssid,
                                 ::onc::connection_state::kConnected);
  return;
}

void NetworkingPrivateLinux::DisconnectFromNetwork(const std::string& guid,
                                                   std::string* error) {
  AssertOnDBusThread();
  std::string device_path_str;
  std::string access_point_path_str;
  std::string ssid;
  DVLOG(1) << "Disconnecting from network GUID " << guid;

  if (!ParseNetworkGuid(guid, &device_path_str, &access_point_path_str,
                        &ssid)) {
    *error = "Invalid Network GUID format";
    return;
  }

  auto network_map = std::make_unique<NetworkMap>();
  GetAllWiFiAccessPoints(false /* configured_only */, false /* visible_only */,
                         0 /* limit */, network_map.get());

  NetworkMap::const_iterator network_iter =
      network_map->find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map->end()) {
    // This network doesn't exist so there's nothing to do.
    return;
  }

  std::string connection_state =
      *network_iter->second.FindString(kAccessPointInfoConnectionState);
  if (connection_state == ::onc::connection_state::kNotConnected) {
    // Already disconnected so nothing to do.
    return;
  }

  // It's not disconnected so disconnect it.
  dbus::ObjectProxy* device_proxy =
      dbus_->GetObjectProxy(networking_private::kNetworkManagerNamespace,
                            dbus::ObjectPath(device_path_str));
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerDeviceNamespace,
      networking_private::kNetworkManagerDisconnectMethod);
  std::unique_ptr<dbus::Response> response(
      device_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(WARNING) << "Failed to disconnect network on device "
                 << device_path_str;
    *error = "Failed to disconnect network";
  }
}

void NetworkingPrivateLinux::StartConnect(const std::string& guid,
                                          VoidCallback success_callback,
                                          FailureCallback failure_callback) {
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("StartConnect", std::move(failure_callback));
    return;
  }

  std::unique_ptr<std::string> error(new std::string);

  // Runs ConnectToNetwork on |dbus_thread|.
  std::string* error_ptr = error.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::ConnectToNetwork,
                     base::Unretained(this), guid, base::Unretained(error_ptr)),
      base::BindOnce(&OnNetworkConnectOperationCompleted, std::move(error),
                     std::move(success_callback), std::move(failure_callback)));
}

void NetworkingPrivateLinux::StartDisconnect(const std::string& guid,
                                             VoidCallback success_callback,
                                             FailureCallback failure_callback) {
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("StartDisconnect", std::move(failure_callback));
    return;
  }

  std::unique_ptr<std::string> error(new std::string);

  // Runs DisconnectFromNetwork on |dbus_thread|.
  std::string* error_ptr = error.get();
  dbus_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::DisconnectFromNetwork,
                     base::Unretained(this), guid, base::Unretained(error_ptr)),
      base::BindOnce(&OnNetworkConnectOperationCompleted, std::move(error),
                     std::move(success_callback), std::move(failure_callback)));
}

void NetworkingPrivateLinux::GetCaptivePortalStatus(
    const std::string& guid,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  ReportNotSupported("GetCaptivePortalStatus", std::move(failure_callback));
}

void NetworkingPrivateLinux::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  ReportNotSupported("UnlockCellularSim", std::move(failure_callback));
}

void NetworkingPrivateLinux::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  ReportNotSupported("SetCellularSimState", std::move(failure_callback));
}

void NetworkingPrivateLinux::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  ReportNotSupported("SelectCellularMobileNetwork",
                     std::move(failure_callback));
}

void NetworkingPrivateLinux::GetEnabledNetworkTypes(
    EnabledNetworkTypesCallback callback) {
  base::Value::List network_list;
  network_list.Append(::onc::network_type::kWiFi);
  std::move(callback).Run(std::move(network_list));
}

void NetworkingPrivateLinux::GetDeviceStateList(
    DeviceStateListCallback callback) {
  DeviceStateList device_state_list;
  api::networking_private::DeviceStateProperties& properties =
      device_state_list.emplace_back();
  properties.type = api::networking_private::NetworkType::kWiFi;
  properties.state = api::networking_private::DeviceStateType::kEnabled;
  std::move(callback).Run(std::move(device_state_list));
}

void NetworkingPrivateLinux::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateLinux ::GetCertificateLists(
    GetCertificateListsCallback callback) {
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateLinux::EnableNetworkType(const std::string& type,
                                               BoolCallback callback) {
  std::move(callback).Run(false);
}

void NetworkingPrivateLinux::DisableNetworkType(const std::string& type,
                                                BoolCallback callback) {
  std::move(callback).Run(false);
}

void NetworkingPrivateLinux::RequestScan(const std::string& /* type */,
                                         BoolCallback callback) {
  std::move(callback).Run(GetNetworksForScanRequest());
}

void NetworkingPrivateLinux::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  network_events_observers_.AddObserver(observer);
}

void NetworkingPrivateLinux::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  network_events_observers_.RemoveObserver(observer);
}

void NetworkingPrivateLinux::OnAccessPointsFound(
    std::unique_ptr<NetworkMap> network_map,
    NetworkListCallback success_callback,
    FailureCallback failure_callback) {
  base::Value::List network_list = CopyNetworkMapToList(*network_map);
  // Give ownership to the member variable.
  network_map_.swap(network_map);
  SendNetworkListChangedEvent(network_list);
  std::move(success_callback).Run(std::move(network_list));
}

void NetworkingPrivateLinux::OnAccessPointsFoundViaScan(
    std::unique_ptr<NetworkMap> network_map) {
  base::Value::List network_list = CopyNetworkMapToList(*network_map);
  // Give ownership to the member variable.
  network_map_.swap(network_map);
  SendNetworkListChangedEvent(network_list);
}

void NetworkingPrivateLinux::SendNetworkListChangedEvent(
    const base::Value::List& network_list) {
  GuidList guidsForEventCallback;

  for (const auto& network : network_list) {
    if (!network.is_dict()) {
      continue;
    }
    if (const std::string* guid =
            network.GetDict().FindString(kAccessPointInfoGuid)) {
      guidsForEventCallback.push_back(*guid);
    }
  }

  OnNetworkListChangedEventOnUIThread(guidsForEventCallback);
}

bool NetworkingPrivateLinux::GetNetworkDevices(
    std::vector<dbus::ObjectPath>* device_paths) {
  AssertOnDBusThread();
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerNamespace,
      networking_private::kNetworkManagerGetDevicesMethod);

  std::unique_ptr<dbus::Response> device_response(
      network_manager_proxy_
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!device_response) {
    return false;
  }

  dbus::MessageReader reader(device_response.get());
  if (!reader.PopArrayOfObjectPaths(device_paths)) {
    LOG(WARNING) << "Unexpected response: " << device_response->ToString();
    return false;
  }

  return true;
}

NetworkingPrivateLinux::DeviceType NetworkingPrivateLinux::GetDeviceType(
    const dbus::ObjectPath& device_path) {
  AssertOnDBusThread();
  dbus::ObjectProxy* device_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, device_path);
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerDeviceNamespace);
  builder.AppendString(networking_private::kNetworkManagerDeviceType);

  std::unique_ptr<dbus::Response> response(
      device_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(ERROR) << "Failed to get the device type for device "
               << device_path.value();
    return NetworkingPrivateLinux::NM_DEVICE_TYPE_UNKNOWN;
  }

  dbus::MessageReader reader(response.get());
  uint32_t device_type = 0;
  if (!reader.PopVariantOfUint32(&device_type)) {
    LOG(ERROR) << "Unexpected response for device " << device_type << ": "
               << response->ToString();
    return NM_DEVICE_TYPE_UNKNOWN;
  }

  return static_cast<NetworkingPrivateLinux::DeviceType>(device_type);
}

void NetworkingPrivateLinux::GetAllWiFiAccessPoints(bool configured_only,
                                                    bool visible_only,
                                                    int limit,
                                                    NetworkMap* network_map) {
  AssertOnDBusThread();
  // TODO(zentaro): The filters are not implemented and are ignored.
  std::vector<dbus::ObjectPath> device_paths;
  if (!GetNetworkDevices(&device_paths)) {
    LOG(ERROR) << "Failed to enumerate network devices";
    return;
  }

  for (const auto& device_path : device_paths) {
    NetworkingPrivateLinux::DeviceType device_type = GetDeviceType(device_path);

    // Get the access points for each WiFi adapter. Other network types are
    // ignored.
    if (device_type != NetworkingPrivateLinux::NM_DEVICE_TYPE_WIFI) {
      continue;
    }

    // Found a wlan adapter
    if (!AddAccessPointsFromDevice(device_path, network_map)) {
      // Ignore devices we can't enumerate.
      LOG(WARNING) << "Failed to add access points from device "
                   << device_path.value();
    }
  }
}

std::unique_ptr<dbus::Response> NetworkingPrivateLinux::GetAccessPointProperty(
    dbus::ObjectProxy* access_point_proxy,
    const std::string& property_name) {
  AssertOnDBusThread();
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerAccessPointNamespace);
  builder.AppendString(property_name);
  std::unique_ptr<dbus::Response> response =
      access_point_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  if (!response) {
    LOG(ERROR) << "Failed to get property for " << property_name;
  }
  return response;
}

bool NetworkingPrivateLinux::GetAccessPointInfo(
    const dbus::ObjectPath& access_point_path,
    base::Value::Dict* access_point_info) {
  AssertOnDBusThread();
  dbus::ObjectProxy* access_point_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, access_point_path);

  // Read the SSID. The GUID is derived from the Ssid.
  {
    std::unique_ptr<dbus::Response> response(GetAccessPointProperty(
        access_point_proxy, networking_private::kNetworkManagerSsidProperty));

    if (!response) {
      return false;
    }

    // The response should contain a variant that contains an array of bytes.
    dbus::MessageReader reader(response.get());
    dbus::MessageReader variant_reader(response.get());
    if (!reader.PopVariant(&variant_reader)) {
      LOG(ERROR) << "Unexpected response for " << access_point_path.value()
                 << ": " << response->ToString();
      return false;
    }

    std::string ssidUTF8;
    if (!variant_reader.PopString(&ssidUTF8)) {
      LOG(ERROR) << "Unexpected response for " << access_point_path.value()
                 << ": " << response->ToString();
      return false;
    }
    std::u16string ssid = base::UTF8ToUTF16(ssidUTF8);
    access_point_info->Set(kAccessPointInfoName, ssid);
  }

  // Read signal strength.
  {
    std::unique_ptr<dbus::Response> response(GetAccessPointProperty(
        access_point_proxy,
        networking_private::kNetworkManagerStrengthProperty));
    if (!response) {
      return false;
    }

    dbus::MessageReader reader(response.get());
    uint8_t strength = 0;
    if (!reader.PopVariantOfByte(&strength)) {
      LOG(ERROR) << "Unexpected response for " << access_point_path.value()
                 << ": " << response->ToString();
      return false;
    }

    access_point_info->SetByDottedPath(kAccessPointInfoWifiSignalStrengthDotted,
                                       strength);
  }

  // Read the security type. This is from the WpaFlags and RsnFlags property
  // which are of the same type and can be OR'd together to find all supported
  // security modes.

  uint32_t wpa_security_flags = 0;
  {
    std::unique_ptr<dbus::Response> response(GetAccessPointProperty(
        access_point_proxy,
        networking_private::kNetworkManagerWpaFlagsProperty));
    if (!response) {
      return false;
    }

    dbus::MessageReader reader(response.get());

    if (!reader.PopVariantOfUint32(&wpa_security_flags)) {
      LOG(ERROR) << "Unexpected response for " << access_point_path.value()
                 << ": " << response->ToString();
      return false;
    }
  }

  uint32_t rsn_security_flags = 0;
  {
    std::unique_ptr<dbus::Response> response(GetAccessPointProperty(
        access_point_proxy,
        networking_private::kNetworkManagerRsnFlagsProperty));
    if (!response) {
      return false;
    }

    dbus::MessageReader reader(response.get());

    if (!reader.PopVariantOfUint32(&rsn_security_flags)) {
      LOG(ERROR) << "Unexpected response for " << access_point_path.value()
                 << ": " << response->ToString();
      return false;
    }
  }

  std::string security;
  MapSecurityFlagsToString(rsn_security_flags | wpa_security_flags, &security);
  access_point_info->SetByDottedPath(kAccessPointInfoWifiSecurityDotted,
                                     security);
  access_point_info->Set(kAccessPointInfoType, kAccessPointInfoTypeWifi);
  access_point_info->Set(kAccessPointInfoConnectable, true);
  return true;
}

bool NetworkingPrivateLinux::AddAccessPointsFromDevice(
    const dbus::ObjectPath& device_path,
    NetworkMap* network_map) {
  AssertOnDBusThread();
  dbus::ObjectPath connected_access_point;
  if (!GetConnectedAccessPoint(device_path, &connected_access_point)) {
    return false;
  }

  dbus::ObjectProxy* device_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, device_path);
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerWirelessDeviceNamespace,
      networking_private::kNetworkManagerGetAccessPointsMethod);
  std::unique_ptr<dbus::Response> response(
      device_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(WARNING) << "Failed to get access points data for "
                 << device_path.value();
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::vector<dbus::ObjectPath> access_point_paths;
  if (!reader.PopArrayOfObjectPaths(&access_point_paths)) {
    LOG(ERROR) << "Unexpected response for " << device_path.value() << ": "
               << response->ToString();
    return false;
  }

  for (const auto& access_point_path : access_point_paths) {
    base::Value::Dict access_point;

    if (GetAccessPointInfo(access_point_path, &access_point)) {
      std::string connection_state =
          (access_point_path == connected_access_point)
              ? ::onc::connection_state::kConnected
              : ::onc::connection_state::kNotConnected;

      access_point.Set(kAccessPointInfoConnectionState, connection_state);
      std::string ssid = *access_point.FindString(kAccessPointInfoName);

      std::string network_guid =
          ConstructNetworkGuid(device_path, access_point_path, ssid);

      // Adds the network to the map. Since each SSID can actually have multiple
      // access point paths, this consolidates them. If it is already
      // in the map it updates the signal strength and GUID paths if this
      // network is stronger or the one that is connected.
      AddOrUpdateAccessPoint(network_map, network_guid, &access_point);
    }
  }

  return true;
}

void NetworkingPrivateLinux::AddOrUpdateAccessPoint(
    NetworkMap* network_map,
    const std::string& network_guid,
    base::Value::Dict* access_point) {
  std::string connection_state =
      *access_point->FindString(kAccessPointInfoConnectionState);
  int signal_strength = *access_point->FindIntByDottedPath(
      kAccessPointInfoWifiSignalStrengthDotted);
  std::u16string ssid =
      base::UTF8ToUTF16(*access_point->FindString(kAccessPointInfoName));
  access_point->Set(kAccessPointInfoGuid, network_guid);

  auto existing_access_point_iter = network_map->find(ssid);

  if (existing_access_point_iter == network_map->end()) {
    // Unseen access point. Add it to the map.
    network_map->insert(NetworkMap::value_type(ssid, std::move(*access_point)));
  } else {
    // Already seen access point. Update the record if this is the connected
    // record or if the signal strength is higher. But don't override a weaker
    // access point if that is the one that is connected.
    base::Value::Dict& existing_access_point =
        existing_access_point_iter->second;
    int existing_signal_strength =
        existing_access_point
            .FindIntByDottedPath(kAccessPointInfoWifiSignalStrengthDotted)
            .value_or(0);

    std::string existing_connection_state =
        *existing_access_point.FindString(kAccessPointInfoConnectionState);

    if ((connection_state == ::onc::connection_state::kConnected) ||
        (!(existing_connection_state == ::onc::connection_state::kConnected) &&
         signal_strength > existing_signal_strength)) {
      existing_access_point.Set(kAccessPointInfoConnectionState,
                                connection_state);
      existing_access_point.SetByDottedPath(
          kAccessPointInfoWifiSignalStrengthDotted, signal_strength);
      existing_access_point.Set(kAccessPointInfoGuid, network_guid);
    }
  }
}

void NetworkingPrivateLinux::MapSecurityFlagsToString(uint32_t security_flags,
                                                      std::string* security) {
  // Valid values are None, WEP-PSK, WEP-8021X, WPA-PSK, WPA-EAP
  if (security_flags == NetworkingPrivateLinux::NM_802_11_AP_SEC_NONE) {
    *security = kAccessPointSecurityNone;
  } else if (security_flags &
             NetworkingPrivateLinux::NM_802_11_AP_SEC_KEY_MGMT_PSK) {
    *security = kAccessPointSecurityWpaPsk;
  } else if (security_flags &
             NetworkingPrivateLinux::NM_802_11_AP_SEC_KEY_MGMT_802_1X) {
    *security = kAccessPointSecurity9021X;
  } else {
    DVLOG(1) << "Security flag mapping is missing. Found " << security_flags;
    *security = kAccessPointSecurityUnknown;
  }

  DVLOG(1) << "Network security setting " << *security;
}

bool NetworkingPrivateLinux::GetConnectedAccessPoint(
    const dbus::ObjectPath& device_path,
    dbus::ObjectPath* access_point_path) {
  AssertOnDBusThread();
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerNamespace);
  builder.AppendString(networking_private::kNetworkManagerActiveConnections);

  std::unique_ptr<dbus::Response> response(
      network_manager_proxy_
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(WARNING) << "Failed to get a list of active connections";
    return false;
  }

  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(response.get());
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  std::vector<dbus::ObjectPath> connection_paths;
  if (!variant_reader.PopArrayOfObjectPaths(&connection_paths)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  for (const auto& connection_path : connection_paths) {
    dbus::ObjectPath connections_device_path;
    if (!GetDeviceOfConnection(connection_path, &connections_device_path)) {
      return false;
    }

    if (connections_device_path == device_path) {
      if (!GetAccessPointForConnection(connection_path, access_point_path)) {
        return false;
      }

      break;
    }
  }

  return true;
}

bool NetworkingPrivateLinux::GetDeviceOfConnection(
    dbus::ObjectPath connection_path,
    dbus::ObjectPath* device_path) {
  AssertOnDBusThread();
  dbus::ObjectProxy* connection_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, connection_path);

  if (!connection_proxy) {
    return false;
  }

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(
      networking_private::kNetworkManagerActiveConnectionNamespace);
  builder.AppendString("Devices");

  std::unique_ptr<dbus::Response> response(
      connection_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(ERROR) << "Failed to get devices";
    return false;
  }

  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(response.get());
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  std::vector<dbus::ObjectPath> device_paths;
  if (!variant_reader.PopArrayOfObjectPaths(&device_paths)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  if (device_paths.size() == 1) {
    *device_path = device_paths[0];

    return true;
  }

  return false;
}

bool NetworkingPrivateLinux::GetAccessPointForConnection(
    dbus::ObjectPath connection_path,
    dbus::ObjectPath* access_point_path) {
  AssertOnDBusThread();
  dbus::ObjectProxy* connection_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, connection_path);

  if (!connection_proxy) {
    return false;
  }

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(
      networking_private::kNetworkManagerActiveConnectionNamespace);
  builder.AppendString(networking_private::kNetworkManagerSpecificObject);

  std::unique_ptr<dbus::Response> response(
      connection_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));

  if (!response) {
    LOG(WARNING) << "Failed to get access point from active connection";
    return false;
  }

  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(response.get());
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  if (!variant_reader.PopObjectPath(access_point_path)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return false;
  }

  return true;
}

bool NetworkingPrivateLinux::SetConnectionStateAndPostEvent(
    const std::string& guid,
    const std::string& ssid,
    const std::string& connection_state) {
  AssertOnDBusThread();

  auto network_iter = network_map_->find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map_->end()) {
    return false;
  }

  DVLOG(1) << "Setting connection state of " << ssid << " to "
           << connection_state;

  // If setting this network to connected, find the previously connected network
  // and disconnect that one. Also retain the guid of that network to fire a
  // changed event.
  std::string* connected_network_guid = nullptr;
  if (connection_state == ::onc::connection_state::kConnected) {
    for (auto& network : *network_map_) {
      if (std::string* other_connection_state =
              network.second.FindString(kAccessPointInfoConnectionState)) {
        if (*other_connection_state == ::onc::connection_state::kConnected) {
          connected_network_guid =
              network.second.FindString(kAccessPointInfoGuid);
          network.second.Set(kAccessPointInfoConnectionState,
                             ::onc::connection_state::kNotConnected);
        }
      }
    }
  }

  // Set the status.
  network_iter->second.Set(kAccessPointInfoConnectionState, connection_state);

  std::unique_ptr<GuidList> changed_networks(new GuidList());
  changed_networks->push_back(guid);

  // Only add a second network if it exists and it is not the same as the
  // network already being added to the list.
  if (connected_network_guid && !connected_network_guid->empty() &&
      *connected_network_guid != guid) {
    changed_networks->push_back(*connected_network_guid);
  }

  PostOnNetworksChangedToUIThread(std::move(changed_networks));
  return true;
}

void NetworkingPrivateLinux::OnNetworksChangedEventOnUIThread(
    const GuidList& network_guids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : network_events_observers_) {
    observer.OnNetworksChangedEvent(network_guids);
  }
}

void NetworkingPrivateLinux::OnNetworkListChangedEventOnUIThread(
    const GuidList& network_guids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : network_events_observers_) {
    observer.OnNetworkListChangedEvent(network_guids);
  }
}

void NetworkingPrivateLinux::PostOnNetworksChangedToUIThread(
    std::unique_ptr<GuidList> guid_list) {
  AssertOnDBusThread();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkingPrivateLinux::OnNetworksChangedEventTask,
                     base::Unretained(this), std::move(guid_list)));
}

void NetworkingPrivateLinux::OnNetworksChangedEventTask(
    std::unique_ptr<GuidList> guid_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnNetworksChangedEventOnUIThread(*guid_list);
}

}  // namespace extensions
