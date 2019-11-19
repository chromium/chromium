// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "components/keyed_service/core/keyed_service.h"
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
  using NetworkMap =
      std::map<base::string16, std::unique_ptr<base::DictionaryValue>>;

  typedef std::vector<std::string> GuidList;

  NetworkingPrivateLinux();

  // NetworkingPrivateDelegate
  void GetProperties(const std::string& guid,
                     const DictionaryCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void GetManagedProperties(const std::string& guid,
                            const DictionaryCallback& success_callback,
                            const FailureCallback& failure_callback) override;
  void GetState(const std::string& guid,
                const DictionaryCallback& success_callback,
                const FailureCallback& failure_callback) override;
  void SetProperties(const std::string& guid,
                     std::unique_ptr<base::DictionaryValue> properties,
                     bool allow_set_shared_config,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void CreateNetwork(bool shared,
                     std::unique_ptr<base::DictionaryValue> properties,
                     const StringCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_config,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   const NetworkListCallback& success_callback,
                   const FailureCallback& failure_callback) override;
  void StartConnect(const std::string& guid,
                    const VoidCallback& success_callback,
                    const FailureCallback& failure_callback) override;
  void StartDisconnect(const std::string& guid,
                       const VoidCallback& success_callback,
                       const FailureCallback& failure_callback) override;
  void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;
  void GetWifiTDLSStatus(const std::string& ip_or_mac_address,
                         const StringCallback& success_callback,
                         const FailureCallback& failure_callback) override;
  void GetCaptivePortalStatus(const std::string& guid,
                              const StringCallback& success_callback,
                              const FailureCallback& failure_callback) override;
  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         const VoidCallback& success_callback,
                         const FailureCallback& failure_callback) override;
  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           const VoidCallback& success_callback,
                           const FailureCallback& failure_callback) override;
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      const VoidCallback& success_callback,
      const FailureCallback& failure_callback) override;
  std::unique_ptr<base::ListValue> GetEnabledNetworkTypes() override;
  std::unique_ptr<DeviceStateList> GetDeviceStateList() override;
  std::unique_ptr<base::DictionaryValue> GetGlobalPolicy() override;
  std::unique_ptr<base::DictionaryValue> GetCertificateLists() override;
  bool EnableNetworkType(const std::string& type) override;
  bool DisableNetworkType(const std::string& type) override;
  bool RequestScan(const std::string& type) override;
  void AddObserver(NetworkingPrivateDelegateObserver* observer) override;
  void RemoveObserver(NetworkingPrivateDelegateObserver* observer) override;

 private:
  ~NetworkingPrivateLinux() override;

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

  // Initializes the DBus instance and the proxy object to the network manager.
  // Must be called on |dbus_thread_|.
  void Initialize();

  // Enumerates all WiFi adapters and scans for access points on each.
  // Results are appended into the provided |network_map|.
  // Must be called on |dbus_thread_|.
  void GetAllWiFiAccessPoints(bool configured_only,
                              bool visible_only,
                              int limit,
                              NetworkMap* network_map);

  // Helper function for handling a scan request. This function acts similarly
  // to the public GetNetworks to get visible networks and fire the
  // OnNetworkListChanged event, however no callbacks are called.
  bool GetNetworksForScanRequest();

  // Initiates the connection to the network.
  // Must be called on |dbus_thread_|.
  void ConnectToNetwork(const std::string& guid, std::string* error);

  // Initiates disconnection from the specified network.
  // Must be called on |dbus_thread_|
  void DisconnectFromNetwork(const std::string& guid, std::string* error);

  // Checks whether the current thread is the DBus thread. If not, DCHECK will
  // fail.
  void AssertOnDBusThread();

  // Verifies that NetworkManager interfaces are initialized.
  // Returns true if NetworkManager is initialized, otherwise returns false
  // and the API call will fail with |kErrorNotSupported|.
  bool CheckNetworkManagerSupported(const FailureCallback& failure_callback);

  // Gets all network devices on the system.
  // Returns false if there is an error getting the device paths.
  bool GetNetworkDevices(std::vector<dbus::ObjectPath>* device_paths);

  // Returns the DeviceType (eg. WiFi, ethernet). corresponding to the
  // |device_path|.
  DeviceType GetDeviceType(const dbus::ObjectPath& device_path);

  // Helper function to enumerate WiFi networks. Takes a path to a Wireless
  // device, scans that device and appends networks to network_list.
  // Returns false if there is an error getting the access points visible
  // to the |device_path|.
  bool AddAccessPointsFromDevice(const dbus::ObjectPath& device_path,
                                 NetworkMap* network_map);

  // Reply callback accepts the map of networks and fires the
  // OnNetworkListChanged event and user callbacks.
  void OnAccessPointsFound(std::unique_ptr<NetworkMap> network_map,
                           const NetworkListCallback& success_callback,
                           const FailureCallback& failure_callback);

  // Reply callback accepts the map of networks and fires the
  // OnNetworkListChanged event.
  void OnAccessPointsFoundViaScan(std::unique_ptr<NetworkMap> network_map);

  // Helper function for OnAccessPointsFound and OnAccessPointsFoundViaScan to
  // fire the OnNetworkListChangedEvent.
  void SendNetworkListChangedEvent(const base::ListValue& network_list);

  // Gets a dictionary of information about the access point.
  // Returns false if there is an error getting information about the
  // supplied |access_point_path|.
  bool GetAccessPointInfo(
      const dbus::ObjectPath& access_point_path,
      const std::unique_ptr<base::DictionaryValue>& access_point_info);

  // Helper function to extract a property from a device.
  // Returns the dbus::Response object from calling Get on the supplied
  // |property_name|.
  std::unique_ptr<dbus::Response> GetAccessPointProperty(
      dbus::ObjectProxy* access_point_proxy,
      const std::string& property_name);

  // If the access_point is not already in the map it is added. Otherwise
  // the access point is updated (eg. with the max of the signal
  // strength).
  void AddOrUpdateAccessPoint(
      NetworkMap* network_map,
      const std::string& network_guid,
      std::unique_ptr<base::DictionaryValue>& access_point);

  // Maps the WPA security flags to a human readable string.
  void MapSecurityFlagsToString(uint32_t securityFlags, std::string* security);

  // Gets the connected access point path on the given device. Internally gets
  // all active connections then checks if the device matches the requested
  // device, then gets the access point associated with the connection.
  // Returns false if there is an error getting the connected access point.
  bool GetConnectedAccessPoint(const dbus::ObjectPath& device_path,
                               dbus::ObjectPath* access_point_path);

  // Given a path to an active connection gets the path to the device
  // that the connection belongs to. Returns false if there is an error getting
  // the device corresponding to the supplied |connection_path|.
  bool GetDeviceOfConnection(dbus::ObjectPath connection_path,
                             dbus::ObjectPath* device_path);

  // Given a path to an active wireless connection gets the path to the
  // access point associated with that connection.
  // Returns false if there is an error getting the |access_point_path|
  // corresponding to the supplied |connection_path|.
  bool GetAccessPointForConnection(dbus::ObjectPath connection_path,
                                   dbus::ObjectPath* access_point_path);

  // Helper method to set the connection state in the |network_map_| and post
  // a change event.
  bool SetConnectionStateAndPostEvent(const std::string& guid,
                                      const std::string& ssid,
                                      const std::string& connection_state);

  // Helper method to post an OnNetworkChanged event to the UI thread from the
  // dbus thread. Used for connection status progress during |StartConnect|.
  void PostOnNetworksChangedToUIThread(std::unique_ptr<GuidList> guid_list);

  // Helper method to be called from the UI thread and manage ownership of the
  // passed vector from the |dbus_thread_|.
  void OnNetworksChangedEventTask(std::unique_ptr<GuidList> guid_list);

  void GetCachedNetworkProperties(const std::string& guid,
                                  base::DictionaryValue* properties,
                                  std::string* error);

  void OnNetworksChangedEventOnUIThread(const GuidList& network_guids);

  void OnNetworkListChangedEventOnUIThread(const GuidList& network_guids);

  // Thread used for DBus actions.
  base::Thread dbus_thread_;
  // DBus instance. Only access on |dbus_thread_|.
  scoped_refptr<dbus::Bus> dbus_;
  // Task runner used by the |dbus_| object.
  scoped_refptr<base::SequencedTaskRunner> dbus_task_runner_;
  // This is owned by |dbus_| object. Only access on |dbus_thread_|.
  dbus::ObjectProxy* network_manager_proxy_;
  // Holds the current mapping of known networks. Only access on |dbus_thread_|.
  std::unique_ptr<NetworkMap> network_map_;
  // Observers to Network Events.
  base::ObserverList<NetworkingPrivateDelegateObserver>::Unchecked
      network_events_observers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateLinux);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LINUX_H_
