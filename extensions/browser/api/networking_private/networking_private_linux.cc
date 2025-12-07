// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_linux.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
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
    NetworkingPrivateDelegate::VoidCallback success_callback,
    NetworkingPrivateDelegate::FailureCallback failure_callback,
    std::string error) {
  if (!error.empty()) {
    std::move(failure_callback).Run(error);
    return;
  }
  std::move(success_callback).Run();
}

}  // namespace

class NetworkingPrivateLinux::GetAllWiFiAccessPointsState
    : public base::RefCounted<GetAllWiFiAccessPointsState> {
 public:
  explicit GetAllWiFiAccessPointsState(
      base::OnceCallback<void(std::unique_ptr<NetworkMap>)> final_callback)
      : callback(std::move(final_callback)),
        network_map(std::make_unique<NetworkMap>()) {}

  base::OnceCallback<void(std::unique_ptr<NetworkMap>)> callback;
  std::unique_ptr<NetworkMap> network_map;

 private:
  friend class base::RefCounted<GetAllWiFiAccessPointsState>;

  ~GetAllWiFiAccessPointsState() {
    if (callback) {
      std::move(callback).Run(std::move(network_map));
    }
  }
};

struct NetworkingPrivateLinux::GetAccessPointInfoState {
  GetAccessPointInfoState(
      const dbus::ObjectPath& access_point_path,
      const dbus::ObjectPath& device_path,
      const dbus::ObjectPath& connected_access_point_path,
      base::OnceCallback<void(std::optional<base::Value::Dict>)> final_callback,
      dbus::ObjectProxy* proxy)
      : access_point_path(access_point_path),
        device_path(device_path),
        connected_access_point_path(connected_access_point_path),
        callback(std::move(final_callback)),
        access_point_proxy(proxy) {}

  ~GetAccessPointInfoState() {
    if (callback) {
      if (failed) {
        std::move(callback).Run(std::nullopt);
      } else {
        std::move(callback).Run(std::move(access_point_info));
      }
    }
  }

  const dbus::ObjectPath access_point_path;
  const dbus::ObjectPath device_path;
  const dbus::ObjectPath connected_access_point_path;
  base::OnceCallback<void(std::optional<base::Value::Dict>)> callback;
  raw_ptr<dbus::ObjectProxy> access_point_proxy;
  base::Value::Dict access_point_info;
  uint32_t wpa_security_flags = 0;
  bool failed = false;
};

class NetworkingPrivateLinux::GetConnectedAccessPointState
    : public base::RefCounted<GetConnectedAccessPointState> {
 public:
  GetConnectedAccessPointState(
      const dbus::ObjectPath& device_path,
      base::OnceCallback<void(dbus::ObjectPath)> final_callback)
      : device_path(device_path), callback(std::move(final_callback)) {}

  const dbus::ObjectPath device_path;
  base::OnceCallback<void(dbus::ObjectPath)> callback;
  std::vector<dbus::ObjectPath> connection_paths;

 private:
  friend class base::RefCounted<GetConnectedAccessPointState>;

  ~GetConnectedAccessPointState() {
    if (callback) {
      std::move(callback).Run(dbus::ObjectPath());
    }
  }
};

NetworkingPrivateLinux::NetworkingPrivateLinux() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus_ = dbus_thread_linux::GetSharedSystemBus();
  network_manager_proxy_ = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace,
      dbus::ObjectPath(networking_private::kNetworkManagerPath));

  if (!network_manager_proxy_) {
    LOG(ERROR) << "Platform does not support NetworkManager over DBUS";
  }
}

NetworkingPrivateLinux::~NetworkingPrivateLinux() = default;

bool NetworkingPrivateLinux::CheckNetworkManagerSupported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return network_manager_proxy_ != nullptr;
}

void NetworkingPrivateLinux::GetProperties(const std::string& guid,
                                           PropertiesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!network_manager_proxy_) {
    LOG(WARNING) << "NetworkManager over DBus is not supported";
    std::move(callback).Run(std::nullopt,
                            extensions::networking_private::kErrorNotSupported);
    return;
  }

  std::string error;
  base::Value::Dict network_properties;

  GetCachedNetworkProperties(guid, &network_properties, &error);

  if (!error.empty()) {
    LOG(ERROR) << "GetCachedNetworkProperties failed: " << error;
    std::move(callback).Run(std::nullopt, error);
    return;
  }
  std::move(callback).Run(std::move(network_properties), std::nullopt);
}

void NetworkingPrivateLinux::GetManagedProperties(const std::string& guid,
                                                  PropertiesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(WARNING) << "GetManagedProperties is not supported";
  std::move(callback).Run(std::nullopt,
                          extensions::networking_private::kErrorNotSupported);
}

void NetworkingPrivateLinux::GetState(const std::string& guid,
                                      DictionaryCallback success_callback,
                                      FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("GetState", std::move(failure_callback));
    return;
  }

  std::string error;
  base::Value::Dict network_properties;
  GetCachedNetworkProperties(guid, &network_properties, &error);

  if (!error.empty()) {
    std::move(failure_callback).Run(error);
    return;
  }
  std::move(success_callback).Run(std::move(network_properties));
}

void NetworkingPrivateLinux::GetCachedNetworkProperties(
    const std::string& guid,
    base::Value::Dict* properties,
    std::string* error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string ssid;

  if (!GuidToSsid(guid, &ssid)) {
    *error = "Invalid Network GUID format";
    return;
  }

  NetworkMap::const_iterator network_iter =
      network_map_.find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map_.end()) {
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("SetProperties", std::move(failure_callback));
}

void NetworkingPrivateLinux::CreateNetwork(bool shared,
                                           base::Value::Dict properties,
                                           StringCallback success_callback,
                                           FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("CreateNetwork", std::move(failure_callback));
}

void NetworkingPrivateLinux::ForgetNetwork(const std::string& guid,
                                           bool allow_forget_shared_config,
                                           VoidCallback success_callback,
                                           FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(zentaro): Implement for Linux.
  ReportNotSupported("ForgetNetwork", std::move(failure_callback));
}

void NetworkingPrivateLinux::GetNetworks(const std::string& network_type,
                                         bool configured_only,
                                         bool visible_only,
                                         int limit,
                                         NetworkListCallback success_callback,
                                         FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("GetNetworks", std::move(failure_callback));
    return;
  }

  if (!(network_type == ::onc::network_type::kWiFi ||
        network_type == ::onc::network_type::kWireless ||
        network_type == ::onc::network_type::kAllTypes)) {
    // Only enumerating WiFi networks is supported on linux.
    ReportNotSupported("GetNetworks with network_type=" + network_type,
                       std::move(failure_callback));
    return;
  }

  // Runs GetAllWiFiAccessPoints and returns the
  // results back to OnAccessPointsFound where the callback is fired.
  GetAllWiFiAccessPoints(
      configured_only, visible_only, limit,
      base::BindOnce(&NetworkingPrivateLinux::OnAccessPointsFound,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(failure_callback)));
}

bool NetworkingPrivateLinux::GetNetworksForScanRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!network_manager_proxy_) {
    return false;
  }

  // Runs GetAllWiFiAccessPoints and returns the
  // results back to SendNetworkListChangedEvent to fire the event. No
  // callbacks are used in this case.
  GetAllWiFiAccessPoints(
      false /* configured_only */, false /* visible_only */, 0 /* limit */,
      base::BindOnce(&NetworkingPrivateLinux::OnAccessPointsFoundViaScan,
                     weak_ptr_factory_.GetWeakPtr()));

  return true;
}

// Constructs the network configuration message and connects to the network.
// The message is of the form:
// {
//   '802-11-wireless': {
//     'ssid': 'FooNetwork'
//   }
// }
void NetworkingPrivateLinux::ConnectToNetwork(
    const std::string& guid,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string device_path_str;
  std::string access_point_path_str;
  std::string ssid;
  DVLOG(1) << "Connecting to network GUID " << guid;

  if (!ParseNetworkGuid(guid, &device_path_str, &access_point_path_str,
                        &ssid)) {
    std::move(callback).Run("Invalid Network GUID format");
    return;
  }

  // Set the connection state to connecting in the map.
  if (!SetConnectionStateAndPostEvent(guid, ssid,
                                      ::onc::connection_state::kConnecting)) {
    std::move(callback).Run("Unknown network GUID");
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

  network_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnConnectToNetworkResponse,
                     weak_ptr_factory_.GetWeakPtr(), guid, ssid,
                     std::move(callback)));
}

void NetworkingPrivateLinux::OnConnectToNetworkResponse(
    const std::string& guid,
    const std::string& ssid,
    base::OnceCallback<void(std::string)> callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(ERROR) << "Failed to add a new connection";

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    std::move(callback).Run("Failed to connect.");
    return;
  }

  dbus::MessageReader reader(response);
  dbus::ObjectPath connection_settings_path;
  dbus::ObjectPath active_connection_path;

  if (!reader.PopObjectPath(&connection_settings_path)) {
    LOG(ERROR) << "Unexpected response for add connection path "
               << ": " << response->ToString();

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    std::move(callback).Run("Failed to connect.");
    return;
  }

  if (!reader.PopObjectPath(&active_connection_path)) {
    LOG(ERROR) << "Unexpected response for connection path "
               << ": " << response->ToString();

    // Set the connection state to NotConnected in the map.
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    std::move(callback).Run("Failed to connect.");
    return;
  }

  // Set the connection state to Connected in the map.
  SetConnectionStateAndPostEvent(guid, ssid,
                                 ::onc::connection_state::kConnected);
  std::move(callback).Run("");
}

void NetworkingPrivateLinux::DisconnectFromNetwork(
    const std::string& guid,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string device_path_str;
  std::string access_point_path_str;
  std::string ssid;
  DVLOG(1) << "Disconnecting from network GUID " << guid;

  if (!ParseNetworkGuid(guid, &device_path_str, &access_point_path_str,
                        &ssid)) {
    std::move(callback).Run("Invalid Network GUID format");
    return;
  }

  NetworkMap::const_iterator network_iter =
      network_map_.find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map_.end()) {
    // This network doesn't exist so there's nothing to do.
    std::move(callback).Run("");
    return;
  }

  std::string connection_state =
      *network_iter->second.FindString(kAccessPointInfoConnectionState);
  if (connection_state == ::onc::connection_state::kNotConnected) {
    // Already disconnected so nothing to do.
    std::move(callback).Run("");
    return;
  }

  // It's not disconnected so disconnect it.
  dbus::ObjectProxy* device_proxy =
      dbus_->GetObjectProxy(networking_private::kNetworkManagerNamespace,
                            dbus::ObjectPath(device_path_str));
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerDeviceNamespace,
      networking_private::kNetworkManagerDisconnectMethod);
  device_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnDisconnectResponse,
                     weak_ptr_factory_.GetWeakPtr(), guid, ssid,
                     std::move(callback)));
}

void NetworkingPrivateLinux::OnDisconnectResponse(
    const std::string& guid,
    const std::string& ssid,
    base::OnceCallback<void(std::string)> callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(WARNING) << "Failed to disconnect network on device " << guid;
    std::move(callback).Run("Failed to disconnect network");
  } else {
    SetConnectionStateAndPostEvent(guid, ssid,
                                   ::onc::connection_state::kNotConnected);
    std::move(callback).Run("");
  }
}

void NetworkingPrivateLinux::StartConnect(const std::string& guid,
                                          VoidCallback success_callback,
                                          FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("StartConnect", std::move(failure_callback));
    return;
  }

  ConnectToNetwork(guid, base::BindOnce(&OnNetworkConnectOperationCompleted,
                                        std::move(success_callback),
                                        std::move(failure_callback)));
}

void NetworkingPrivateLinux::StartDisconnect(const std::string& guid,
                                             VoidCallback success_callback,
                                             FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CheckNetworkManagerSupported()) {
    ReportNotSupported("StartDisconnect", std::move(failure_callback));
    return;
  }

  DisconnectFromNetwork(
      guid,
      base::BindOnce(&OnNetworkConnectOperationCompleted,
                     std::move(success_callback), std::move(failure_callback)));
}

void NetworkingPrivateLinux::GetCaptivePortalStatus(
    const std::string& guid,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("GetCaptivePortalStatus", std::move(failure_callback));
}

void NetworkingPrivateLinux::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("UnlockCellularSim", std::move(failure_callback));
}

void NetworkingPrivateLinux::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("SetCellularSimState", std::move(failure_callback));
}

void NetworkingPrivateLinux::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportNotSupported("SelectCellularMobileNetwork",
                     std::move(failure_callback));
}

void NetworkingPrivateLinux::GetEnabledNetworkTypes(
    EnabledNetworkTypesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List network_list;
  network_list.Append(::onc::network_type::kWiFi);
  std::move(callback).Run(std::move(network_list));
}

void NetworkingPrivateLinux::GetDeviceStateList(
    DeviceStateListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DeviceStateList device_state_list;
  api::networking_private::DeviceStateProperties& properties =
      device_state_list.emplace_back();
  properties.type = api::networking_private::NetworkType::kWiFi;
  properties.state = api::networking_private::DeviceStateType::kEnabled;
  std::move(callback).Run(std::move(device_state_list));
}

void NetworkingPrivateLinux::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateLinux ::GetCertificateLists(
    GetCertificateListsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(base::Value::Dict());
}

void NetworkingPrivateLinux::EnableNetworkType(const std::string& type,
                                               BoolCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(false);
}

void NetworkingPrivateLinux::DisableNetworkType(const std::string& type,
                                                BoolCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(false);
}

void NetworkingPrivateLinux::RequestScan(const std::string& /* type */,
                                         BoolCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(GetNetworksForScanRequest());
}

void NetworkingPrivateLinux::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network_events_observers_.AddObserver(observer);
}

void NetworkingPrivateLinux::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network_events_observers_.RemoveObserver(observer);
}

void NetworkingPrivateLinux::OnAccessPointsFound(
    NetworkListCallback success_callback,
    FailureCallback failure_callback,
    std::unique_ptr<NetworkMap> network_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!network_map) {
    std::move(failure_callback).Run("Failed to get network list.");
    return;
  }
  base::Value::List network_list = CopyNetworkMapToList(*network_map);
  // Give ownership to the member variable.
  network_map_.swap(*network_map);
  SendNetworkListChangedEvent(network_list);
  std::move(success_callback).Run(std::move(network_list));
}

void NetworkingPrivateLinux::OnAccessPointsFoundViaScan(
    std::unique_ptr<NetworkMap> network_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!network_map) {
    return;
  }
  base::Value::List network_list = CopyNetworkMapToList(*network_map);
  // Give ownership to the member variable.
  network_map_.swap(*network_map);
  SendNetworkListChangedEvent(network_list);
}

void NetworkingPrivateLinux::SendNetworkListChangedEvent(
    const base::Value::List& network_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GuidList guids_for_event_callback;

  for (const auto& network : network_list) {
    if (!network.is_dict()) {
      continue;
    }
    if (const std::string* guid =
            network.GetDict().FindString(kAccessPointInfoGuid)) {
      guids_for_event_callback.push_back(*guid);
    }
  }

  for (auto& observer : network_events_observers_) {
    observer.OnNetworkListChangedEvent(guids_for_event_callback);
  }
}

void NetworkingPrivateLinux::GetNetworkDevices(
    base::OnceCallback<void(std::optional<std::vector<dbus::ObjectPath>>)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerNamespace,
      networking_private::kNetworkManagerGetDevicesMethod);

  network_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnGetNetworkDevicesResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkingPrivateLinux::OnGetNetworkDevicesResponse(
    base::OnceCallback<void(std::optional<std::vector<dbus::ObjectPath>>)>
        callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::vector<dbus::ObjectPath> device_paths;
  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfObjectPaths(&device_paths)) {
    LOG(WARNING) << "Unexpected response: " << response->ToString();
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(device_paths));
}

void NetworkingPrivateLinux::GetDeviceType(
    const dbus::ObjectPath& device_path,
    base::OnceCallback<void(std::optional<DeviceType>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* device_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, device_path);
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerDeviceNamespace);
  builder.AppendString(networking_private::kNetworkManagerDeviceType);

  device_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnGetDeviceTypeResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkingPrivateLinux::OnGetDeviceTypeResponse(
    base::OnceCallback<void(std::optional<DeviceType>)> callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(ERROR) << "Failed to get the device type for device";
    std::move(callback).Run(NM_DEVICE_TYPE_UNKNOWN);
    return;
  }

  dbus::MessageReader reader(response);
  uint32_t device_type = 0;
  if (!reader.PopVariantOfUint32(&device_type)) {
    LOG(ERROR) << "Unexpected response for device type: "
               << response->ToString();
    std::move(callback).Run(NM_DEVICE_TYPE_UNKNOWN);
    return;
  }

  std::move(callback).Run(static_cast<DeviceType>(device_type));
}

void NetworkingPrivateLinux::GetAllWiFiAccessPoints(
    bool configured_only,
    bool visible_only,
    int limit,
    base::OnceCallback<void(std::unique_ptr<NetworkMap>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(zentaro): The filters are not implemented and are ignored.
  auto state =
      base::MakeRefCounted<GetAllWiFiAccessPointsState>(std::move(callback));

  GetNetworkDevices(base::BindOnce(
      &NetworkingPrivateLinux::OnGetNetworkDevicesForGetAllWiFiAccessPoints,
      weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetNetworkDevicesForGetAllWiFiAccessPoints(
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    std::optional<std::vector<dbus::ObjectPath>> device_paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!device_paths) {
    LOG(ERROR) << "Failed to enumerate network devices";
    return;
  }

  if (device_paths->empty()) {
    return;
  }

  for (const auto& device_path : *device_paths) {
    GetDeviceType(
        device_path,
        base::BindOnce(
            &NetworkingPrivateLinux::OnGetDeviceTypeForGetAllWiFiAccessPoints,
            weak_ptr_factory_.GetWeakPtr(), state, device_path));
  }
}

void NetworkingPrivateLinux::OnGetDeviceTypeForGetAllWiFiAccessPoints(
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    const dbus::ObjectPath& device_path,
    std::optional<DeviceType> device_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!device_type.has_value() ||
      *device_type != NetworkingPrivateLinux::NM_DEVICE_TYPE_WIFI) {
    return;
  }

  // Found a wlan adapter
  AddAccessPointsFromDevice(
      device_path, state,
      base::BindOnce(
          &NetworkingPrivateLinux::OnAddAccessPointsFromDeviceFinished,
          weak_ptr_factory_.GetWeakPtr(), state));
}

void NetworkingPrivateLinux::OnAddAccessPointsFromDeviceFinished(
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    // Ignore devices we can't enumerate.
    LOG(WARNING) << "Failed to add access points from a device.";
  }
}

void NetworkingPrivateLinux::GetAccessPointProperty(
    dbus::ObjectProxy* access_point_proxy,
    const std::string& property_name,
    base::OnceCallback<void(dbus::Response*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerAccessPointNamespace);
  builder.AppendString(property_name);
  access_point_proxy->CallMethod(&method_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                 std::move(callback));
}

void NetworkingPrivateLinux::GetAccessPointInfo(
    const dbus::ObjectPath& access_point_path,
    const dbus::ObjectPath& device_path,
    const dbus::ObjectPath& connected_access_point_path,
    base::OnceCallback<void(std::optional<base::Value::Dict>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* access_point_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, access_point_path);

  auto state = std::make_unique<GetAccessPointInfoState>(
      access_point_path, device_path, connected_access_point_path,
      std::move(callback), access_point_proxy);

  GetAccessPointProperty(
      access_point_proxy, networking_private::kNetworkManagerSsidProperty,
      base::BindOnce(&NetworkingPrivateLinux::OnGetSsidForAccessPointInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetSsidForAccessPointInfo(
    std::unique_ptr<GetAccessPointInfoState> state,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    state->failed = true;
    return;
  }

  // The response should contain a variant that contains an array of bytes.
  dbus::MessageReader reader(response);
  dbus::MessageReader variant_reader(response);
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response for " << state->access_point_path.value()
               << ": " << response->ToString();
    state->failed = true;
    return;
  }

  std::string ssidUTF8;
  if (!variant_reader.PopString(&ssidUTF8)) {
    LOG(ERROR) << "Unexpected response for " << state->access_point_path.value()
               << ": " << response->ToString();
    state->failed = true;
    return;
  }
  state->access_point_info.Set(kAccessPointInfoName,
                               base::UTF8ToUTF16(ssidUTF8));
  std::string network_guid = ConstructNetworkGuid(
      state->device_path, state->access_point_path, ssidUTF8);
  state->access_point_info.Set(kAccessPointInfoGuid, network_guid);

  auto* access_point_proxy = state->access_point_proxy.get();
  GetAccessPointProperty(
      access_point_proxy, networking_private::kNetworkManagerStrengthProperty,
      base::BindOnce(&NetworkingPrivateLinux::OnGetStrengthForAccessPointInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetStrengthForAccessPointInfo(
    std::unique_ptr<GetAccessPointInfoState> state,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    state->failed = true;
    return;
  }

  dbus::MessageReader reader(response);
  uint8_t strength = 0;
  if (!reader.PopVariantOfByte(&strength)) {
    LOG(ERROR) << "Unexpected response for " << state->access_point_path.value()
               << ": " << response->ToString();
    state->failed = true;
    return;
  }
  state->access_point_info.SetByDottedPath(
      kAccessPointInfoWifiSignalStrengthDotted, strength);

  auto* access_point_proxy = state->access_point_proxy.get();
  GetAccessPointProperty(
      access_point_proxy, networking_private::kNetworkManagerWpaFlagsProperty,
      base::BindOnce(&NetworkingPrivateLinux::OnGetWpaFlagsForAccessPointInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetWpaFlagsForAccessPointInfo(
    std::unique_ptr<GetAccessPointInfoState> state,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    state->failed = true;
    return;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopVariantOfUint32(&state->wpa_security_flags)) {
    LOG(ERROR) << "Unexpected response for " << state->access_point_path.value()
               << ": " << response->ToString();
    state->failed = true;
    return;
  }

  auto* access_point_proxy = state->access_point_proxy.get();
  GetAccessPointProperty(
      access_point_proxy, networking_private::kNetworkManagerRsnFlagsProperty,
      base::BindOnce(&NetworkingPrivateLinux::OnGetRsnFlagsForAccessPointInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetRsnFlagsForAccessPointInfo(
    std::unique_ptr<GetAccessPointInfoState> state,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    state->failed = true;
    return;
  }

  dbus::MessageReader reader(response);
  uint32_t rsn_security_flags = 0;
  if (!reader.PopVariantOfUint32(&rsn_security_flags)) {
    LOG(ERROR) << "Unexpected response for " << state->access_point_path.value()
               << ": " << response->ToString();
    state->failed = true;
    return;
  }

  std::string security;
  MapSecurityFlagsToString(rsn_security_flags | state->wpa_security_flags,
                           &security);
  state->access_point_info.SetByDottedPath(kAccessPointInfoWifiSecurityDotted,
                                           security);
  state->access_point_info.Set(kAccessPointInfoType, kAccessPointInfoTypeWifi);
  state->access_point_info.Set(kAccessPointInfoConnectable, true);
  std::string connection_state =
      (state->access_point_path == state->connected_access_point_path)
          ? ::onc::connection_state::kConnected
          : ::onc::connection_state::kNotConnected;
  state->access_point_info.Set(kAccessPointInfoConnectionState,
                               connection_state);
}

void NetworkingPrivateLinux::AddAccessPointsFromDevice(
    const dbus::ObjectPath& device_path,
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetConnectedAccessPoint(
      device_path,
      base::BindOnce(
          &NetworkingPrivateLinux::OnGetConnectedAccessPointForAddAccessPoints,
          weak_ptr_factory_.GetWeakPtr(), device_path, state,
          std::move(callback)));
}

void NetworkingPrivateLinux::OnGetConnectedAccessPointForAddAccessPoints(
    const dbus::ObjectPath& device_path,
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    base::OnceCallback<void(bool)> callback,
    dbus::ObjectPath connected_access_point) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* device_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, device_path);
  dbus::MethodCall method_call(
      networking_private::kNetworkManagerWirelessDeviceNamespace,
      networking_private::kNetworkManagerGetAccessPointsMethod);
  device_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnGetAccessPointsForDevice,
                     weak_ptr_factory_.GetWeakPtr(), device_path, state,
                     std::move(callback), std::move(connected_access_point)));
}

void NetworkingPrivateLinux::OnGetAccessPointsForDevice(
    const dbus::ObjectPath& device_path,
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    base::OnceCallback<void(bool)> callback,
    dbus::ObjectPath connected_access_point,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(WARNING) << "Failed to get access points data for "
                 << device_path.value();
    std::move(callback).Run(false);
    return;
  }

  dbus::MessageReader reader(response);
  std::vector<dbus::ObjectPath> access_point_paths;
  if (!reader.PopArrayOfObjectPaths(&access_point_paths)) {
    LOG(ERROR) << "Unexpected response for " << device_path.value() << ": "
               << response->ToString();
    std::move(callback).Run(false);
    return;
  }

  if (access_point_paths.empty()) {
    std::move(callback).Run(true);
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      access_point_paths.size(), base::BindOnce(std::move(callback), true));

  for (const auto& access_point_path : access_point_paths) {
    GetAccessPointInfo(
        access_point_path, device_path, connected_access_point,
        base::BindOnce(&NetworkingPrivateLinux::OnGetAccessPointInfo,
                       weak_ptr_factory_.GetWeakPtr(), state, barrier));
  }
}

void NetworkingPrivateLinux::OnGetAccessPointInfo(
    scoped_refptr<GetAllWiFiAccessPointsState> state,
    base::RepeatingClosure finished_callback,
    std::optional<base::Value::Dict> access_point_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (access_point_info) {
    AddOrUpdateAccessPoint(state->network_map.get(),
                           std::move(*access_point_info));
  }
  finished_callback.Run();
}

void NetworkingPrivateLinux::AddOrUpdateAccessPoint(
    NetworkMap* network_map,
    base::Value::Dict access_point) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(access_point.FindString(kAccessPointInfoGuid));
  std::string connection_state =
      *access_point.FindString(kAccessPointInfoConnectionState);
  int signal_strength =
      access_point.FindIntByDottedPath(kAccessPointInfoWifiSignalStrengthDotted)
          .value_or(0);
  std::u16string ssid =
      base::UTF8ToUTF16(*access_point.FindString(kAccessPointInfoName));

  auto existing_access_point_iter = network_map->find(ssid);

  if (existing_access_point_iter == network_map->end()) {
    // Unseen access point. Add it to the map.
    network_map->insert(NetworkMap::value_type(ssid, std::move(access_point)));
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
      existing_access_point.Set(kAccessPointInfoGuid,
                                *access_point.FindString(kAccessPointInfoGuid));
    }
  }
}

void NetworkingPrivateLinux::MapSecurityFlagsToString(uint32_t security_flags,
                                                      std::string* security) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

void NetworkingPrivateLinux::GetConnectedAccessPoint(
    const dbus::ObjectPath& device_path,
    base::OnceCallback<void(dbus::ObjectPath)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnGetActiveConnections(base::MakeRefCounted<GetConnectedAccessPointState>(
      device_path, std::move(callback)));
}

void NetworkingPrivateLinux::OnGetActiveConnections(
    scoped_refptr<GetConnectedAccessPointState> state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(networking_private::kNetworkManagerNamespace);
  builder.AppendString(networking_private::kNetworkManagerActiveConnections);

  network_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnGetActiveConnectionsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void NetworkingPrivateLinux::OnGetActiveConnectionsResponse(
    scoped_refptr<GetConnectedAccessPointState> state,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(WARNING) << "Failed to get a list of active connections";
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader variant_reader(response);
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return;
  }

  if (!variant_reader.PopArrayOfObjectPaths(&state->connection_paths)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    return;
  }

  if (state->connection_paths.empty()) {
    return;
  }

  for (const auto& connection_path : state->connection_paths) {
    GetDeviceOfConnection(
        connection_path,
        base::BindOnce(&NetworkingPrivateLinux::OnGetDeviceOfConnection,
                       weak_ptr_factory_.GetWeakPtr(), state, connection_path));
  }
}

void NetworkingPrivateLinux::OnGetDeviceOfConnection(
    scoped_refptr<GetConnectedAccessPointState> state,
    const dbus::ObjectPath& connection_path,
    dbus::ObjectPath device_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_path == state->device_path) {
    GetAccessPointForConnection(
        connection_path,
        base::BindOnce(&NetworkingPrivateLinux::OnGetAccessPointForConnection,
                       weak_ptr_factory_.GetWeakPtr(), state));
    return;
  }
}

void NetworkingPrivateLinux::OnGetAccessPointForConnection(
    scoped_refptr<GetConnectedAccessPointState> state,
    dbus::ObjectPath access_point_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!access_point_path.value().empty() && state->callback) {
    std::move(state->callback).Run(access_point_path);
  }
}

void NetworkingPrivateLinux::GetDeviceOfConnection(
    dbus::ObjectPath connection_path,
    base::OnceCallback<void(dbus::ObjectPath)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* connection_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, connection_path);

  if (!connection_proxy) {
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(
      networking_private::kNetworkManagerActiveConnectionNamespace);
  builder.AppendString("Devices");

  connection_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&NetworkingPrivateLinux::OnGetDeviceOfConnectionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkingPrivateLinux::OnGetDeviceOfConnectionResponse(
    base::OnceCallback<void(dbus::ObjectPath)> callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(ERROR) << "Failed to get devices";
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader variant_reader(response);
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  std::vector<dbus::ObjectPath> device_paths;
  if (!variant_reader.PopArrayOfObjectPaths(&device_paths)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  if (device_paths.size() == 1) {
    std::move(callback).Run(device_paths[0]);
  } else {
    std::move(callback).Run(dbus::ObjectPath());
  }
}

void NetworkingPrivateLinux::GetAccessPointForConnection(
    dbus::ObjectPath connection_path,
    base::OnceCallback<void(dbus::ObjectPath)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::ObjectProxy* connection_proxy = dbus_->GetObjectProxy(
      networking_private::kNetworkManagerNamespace, connection_path);

  if (!connection_proxy) {
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES,
                               networking_private::kNetworkManagerGetMethod);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(
      networking_private::kNetworkManagerActiveConnectionNamespace);
  builder.AppendString(networking_private::kNetworkManagerSpecificObject);

  connection_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          &NetworkingPrivateLinux::OnGetAccessPointForConnectionResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkingPrivateLinux::OnGetAccessPointForConnectionResponse(
    base::OnceCallback<void(dbus::ObjectPath)> callback,
    dbus::Response* response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(WARNING) << "Failed to get access point from active connection";
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader variant_reader(response);
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  dbus::ObjectPath access_point_path;
  if (!variant_reader.PopObjectPath(&access_point_path)) {
    LOG(ERROR) << "Unexpected response: " << response->ToString();
    std::move(callback).Run(dbus::ObjectPath());
    return;
  }

  std::move(callback).Run(access_point_path);
}

bool NetworkingPrivateLinux::SetConnectionStateAndPostEvent(
    const std::string& guid,
    const std::string& ssid,
    const std::string& connection_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto network_iter = network_map_.find(base::UTF8ToUTF16(ssid));
  if (network_iter == network_map_.end()) {
    return false;
  }

  DVLOG(1) << "Setting connection state of " << ssid << " to "
           << connection_state;

  // If setting this network to connected, find the previously connected network
  // and disconnect that one. Also retain the guid of that network to fire a
  // changed event.
  std::string* connected_network_guid = nullptr;
  if (connection_state == ::onc::connection_state::kConnected) {
    for (auto& network : network_map_) {
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

  auto changed_networks = std::make_unique<GuidList>();
  changed_networks->push_back(guid);

  // Only add a second network if it exists and it is not the same as the
  // network already being added to the list.
  if (connected_network_guid && !connected_network_guid->empty() &&
      *connected_network_guid != guid) {
    changed_networks->push_back(*connected_network_guid);
  }

  for (auto& observer : network_events_observers_) {
    observer.OnNetworksChangedEvent(*changed_networks);
  }
  return true;
}

}  // namespace extensions
