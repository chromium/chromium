// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace dbus {
class Bus;
class ObjectPath;
class ObjectProxy;
class Response;
}  // namespace dbus

namespace extensions {

// Linux NetworkingPrivateDelegate implementation.
class NetworkingPrivateLinux : public NetworkingPrivateDelegate {
 public:
  using GuidList = std::vector<std::string>;
  using NetworkMap = std::map<std::u16string, base::Value::Dict>;

  NetworkingPrivateLinux();
  ~NetworkingPrivateLinux() override;

  NetworkingPrivateLinux(const NetworkingPrivateLinux&) = delete;
  NetworkingPrivateLinux& operator=(const NetworkingPrivateLinux&) = delete;

  // NetworkingPrivateDelegate
  void GetProperties(const std::string& guid,
                     PropertiesCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            PropertiesCallback callback) override;
  void GetState(const std::string& guid,
                DictionaryCallback success_callback,
                FailureCallback failure_callback) override;
  void SetProperties(const std::string& guid,
                     base::Value::Dict properties,
                     bool allow_set_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void CreateNetwork(bool shared,
                     base::Value::Dict properties,
                     StringCallback success_callback,
                     FailureCallback failure_callback) override;
  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   NetworkListCallback success_callback,
                   FailureCallback failure_callback) override;
  void StartConnect(const std::string& guid,
                    VoidCallback success_callback,
                    FailureCallback failure_callback) override;
  void StartDisconnect(const std::string& guid,
                       VoidCallback success_callback,
                       FailureCallback failure_callback) override;
  void GetCaptivePortalStatus(const std::string& guid,
                              StringCallback success_callback,
                              FailureCallback failure_callback) override;
  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         VoidCallback success_callback,
                         FailureCallback failure_callback) override;
  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           VoidCallback success_callback,
                           FailureCallback failure_callback) override;
  void SelectCellularMobileNetwork(const std::string& guid,
                                   const std::string& network_id,
                                   VoidCallback success_callback,
                                   FailureCallback failure_callback) override;
  void GetEnabledNetworkTypes(EnabledNetworkTypesCallback callback) override;
  void GetDeviceStateList(DeviceStateListCallback callback) override;
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void GetCertificateLists(GetCertificateListsCallback callback) override;
  void EnableNetworkType(const std::string& type,
                         BoolCallback callback) override;
  void DisableNetworkType(const std::string& type,
                          BoolCallback callback) override;
  void RequestScan(const std::string& type, BoolCallback callback) override;
  void AddObserver(NetworkingPrivateDelegateObserver* observer) override;
  void RemoveObserver(NetworkingPrivateDelegateObserver* observer) override;

 private:
  // https://developer.gnome.org/NetworkManager/unstable/spec.html#type-NM_DEVICE_TYPE
  enum DeviceType {
    NM_DEVICE_TYPE_UNKNOWN = 0,
    NM_DEVICE_TYPE_ETHERNET = 1,
    NM_DEVICE_TYPE_WIFI = 2
  };

  // https://developer.gnome.org/NetworkManager/unstable/spec.html#type-NM_802_11_AP_SEC
  enum WirelessSecurityFlags {
    NM_802_11_AP_SEC_NONE = 0x0,
    NM_802_11_AP_SEC_PAIR_WEP40 = 0x1,
    NM_802_11_AP_SEC_PAIR_WEP104 = 0x2,
    NM_802_11_AP_SEC_PAIR_TKIP = 0x4,
    NM_802_11_AP_SEC_PAIR_CCMP = 0x8,
    NM_802_11_AP_SEC_GROUP_WEP40 = 0x10,
    NM_802_11_AP_SEC_GROUP_WEP104 = 0x20,
    NM_802_11_AP_SEC_GROUP_TKIP = 0x40,
    NM_802_11_AP_SEC_GROUP_CCMP = 0x80,
    NM_802_11_AP_SEC_KEY_MGMT_PSK = 0x100,
    NM_802_11_AP_SEC_KEY_MGMT_802_1X = 0x200
  };

  struct GetAccessPointInfoState;
  class GetAllWiFiAccessPointsState;
  class GetConnectedAccessPointState;

  // Enumerates all WiFi adapters and scans for access points on each.
  // Results are appended into the provided |network_map|.
  void GetAllWiFiAccessPoints(
      bool configured_only,
      bool visible_only,
      int limit,
      base::OnceCallback<void(std::unique_ptr<NetworkMap>)> callback);
  void OnGetNetworkDevicesForGetAllWiFiAccessPoints(
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      std::optional<std::vector<dbus::ObjectPath>> device_paths);
  void OnGetDeviceTypeForGetAllWiFiAccessPoints(
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      const dbus::ObjectPath& device_path,
      std::optional<DeviceType> device_type);
  void OnAddAccessPointsFromDeviceFinished(
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      bool success);

  // Helper function for handling a scan request. This function acts similarly
  // to the public GetNetworks to get visible networks and fire the
  // OnNetworkListChanged event, however no callbacks are called.
  bool GetNetworksForScanRequest();

  // Initiates the connection to the network.
  void ConnectToNetwork(const std::string& guid,
                        base::OnceCallback<void(std::string)> callback);
  void OnConnectToNetworkResponse(
      const std::string& guid,
      const std::string& ssid,
      base::OnceCallback<void(std::string)> callback,
      dbus::Response* response);

  // Initiates disconnection from the specified network.
  void DisconnectFromNetwork(const std::string& guid,
                             base::OnceCallback<void(std::string)> callback);
  void OnDisconnectResponse(const std::string& guid,
                            const std::string& ssid,
                            base::OnceCallback<void(std::string)> callback,
                            dbus::Response* response);

  // Verifies that NetworkManager interfaces are initialized.
  // Returns true if NetworkManager is initialized, otherwise returns false
  // and the API call will fail with |kErrorNotSupported|.
  bool CheckNetworkManagerSupported();

  // Gets all network devices on the system.
  void GetNetworkDevices(
      base::OnceCallback<void(std::optional<std::vector<dbus::ObjectPath>>)>
          callback);
  void OnGetNetworkDevicesResponse(
      base::OnceCallback<void(std::optional<std::vector<dbus::ObjectPath>>)>
          callback,
      dbus::Response* response);

  // Returns the DeviceType (eg. WiFi, ethernet). corresponding to the
  // |device_path|.
  void GetDeviceType(
      const dbus::ObjectPath& device_path,
      base::OnceCallback<void(std::optional<DeviceType>)> callback);
  void OnGetDeviceTypeResponse(
      base::OnceCallback<void(std::optional<DeviceType>)> callback,
      dbus::Response* response);

  // Helper function to enumerate WiFi networks. Takes a path to a Wireless
  // device, scans that device and appends networks to network_list.
  void AddAccessPointsFromDevice(
      const dbus::ObjectPath& device_path,
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      base::OnceCallback<void(bool)> callback);
  void OnGetConnectedAccessPointForAddAccessPoints(
      const dbus::ObjectPath& device_path,
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      base::OnceCallback<void(bool)> callback,
      dbus::ObjectPath connected_access_point);
  void OnGetAccessPointsForDevice(
      const dbus::ObjectPath& device_path,
      scoped_refptr<GetAllWiFiAccessPointsState> state,
      base::OnceCallback<void(bool)> callback,
      dbus::ObjectPath connected_access_point,
      dbus::Response* response);
  void OnGetAccessPointInfo(scoped_refptr<GetAllWiFiAccessPointsState> state,
                            base::RepeatingClosure finished_callback,
                            std::optional<base::Value::Dict> access_point_info);

  // Reply callback accepts the map of networks and fires the
  // OnNetworkListChanged event and user callbacks.
  void OnAccessPointsFound(NetworkListCallback success_callback,
                           FailureCallback failure_callback,
                           std::unique_ptr<NetworkMap> network_map);

  // Reply callback accepts the map of networks and fires the
  // OnNetworkListChanged event.
  void OnAccessPointsFoundViaScan(std::unique_ptr<NetworkMap> network_map);

  // Helper function for OnAccessPointsFound and OnAccessPointsFoundViaScan to
  // fire the OnNetworkListChangedEvent.
  void SendNetworkListChangedEvent(const base::Value::List& network_list);

  // Gets a dictionary of information about the access point.
  void GetAccessPointInfo(
      const dbus::ObjectPath& access_point_path,
      const dbus::ObjectPath& device_path,
      const dbus::ObjectPath& connected_access_point_path,
      base::OnceCallback<void(std::optional<base::Value::Dict>)> callback);
  void OnGetSsidForAccessPointInfo(
      std::unique_ptr<GetAccessPointInfoState> state,
      dbus::Response* response);
  void OnGetStrengthForAccessPointInfo(
      std::unique_ptr<GetAccessPointInfoState> state,
      dbus::Response* response);
  void OnGetWpaFlagsForAccessPointInfo(
      std::unique_ptr<GetAccessPointInfoState> state,
      dbus::Response* response);
  void OnGetRsnFlagsForAccessPointInfo(
      std::unique_ptr<GetAccessPointInfoState> state,
      dbus::Response* response);

  // Helper function to extract a property from a device.
  // Returns the dbus::Response object from calling Get on the supplied
  // |property_name|.
  void GetAccessPointProperty(
      dbus::ObjectProxy* access_point_proxy,
      const std::string& property_name,
      base::OnceCallback<void(dbus::Response*)> callback);

  // If the access_point is not already in the map it is added. Otherwise
  // the access point is updated (eg. with the max of the signal
  // strength).
  void AddOrUpdateAccessPoint(NetworkMap* network_map,
                              base::Value::Dict access_point);

  // Maps the WPA security flags to a human readable string.
  void MapSecurityFlagsToString(uint32_t securityFlags, std::string* security);

  // Gets the connected access point path on the given device. Internally gets
  // all active connections then checks if the device matches the requested
  // device, then gets the access point associated with the connection.
  void GetConnectedAccessPoint(
      const dbus::ObjectPath& device_path,
      base::OnceCallback<void(dbus::ObjectPath)> callback);
  void OnGetActiveConnections(
      scoped_refptr<GetConnectedAccessPointState> state);
  void OnGetActiveConnectionsResponse(
      scoped_refptr<GetConnectedAccessPointState> state,
      dbus::Response* response);
  void OnGetDeviceOfConnection(
      scoped_refptr<GetConnectedAccessPointState> state,
      const dbus::ObjectPath& connection_path,
      dbus::ObjectPath device_path);
  void OnGetAccessPointForConnection(
      scoped_refptr<GetConnectedAccessPointState> state,
      dbus::ObjectPath access_point_path);

  // Given a path to an active connection gets the path to the device
  // that the connection belongs to.
  void GetDeviceOfConnection(
      dbus::ObjectPath connection_path,
      base::OnceCallback<void(dbus::ObjectPath)> callback);
  void OnGetDeviceOfConnectionResponse(
      base::OnceCallback<void(dbus::ObjectPath)> callback,
      dbus::Response* response);

  // Given a path to an active wireless connection gets the path to the
  // access point associated with that connection.
  void GetAccessPointForConnection(
      dbus::ObjectPath connection_path,
      base::OnceCallback<void(dbus::ObjectPath)> callback);
  void OnGetAccessPointForConnectionResponse(
      base::OnceCallback<void(dbus::ObjectPath)> callback,
      dbus::Response* response);

  // Helper method to set the connection state in the |network_map_| and post
  // a change event.
  bool SetConnectionStateAndPostEvent(const std::string& guid,
                                      const std::string& ssid,
                                      const std::string& connection_state);

  void GetCachedNetworkProperties(const std::string& guid,
                                  base::Value::Dict* properties,
                                  std::string* error);

  // DBus instance.
  scoped_refptr<dbus::Bus> dbus_;
  // This is owned by |dbus_| object.
  raw_ptr<dbus::ObjectProxy> network_manager_proxy_ = nullptr;
  // Holds the current mapping of known networks.
  NetworkMap network_map_;
  // Observers to Network Events.
  base::ObserverList<NetworkingPrivateDelegateObserver>::Unchecked
      network_events_observers_;

  base::WeakPtrFactory<NetworkingPrivateLinux> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_
