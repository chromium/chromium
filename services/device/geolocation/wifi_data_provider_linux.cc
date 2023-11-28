// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for suitable for typical linux distributions.
// Currently, only the NetworkManager API is used, accessed via D-Bus (in turn
// accessed via the GLib wrapper).

#include "services/device/geolocation/wifi_data_provider_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {
namespace {
// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;           // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;      // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000;            // 20s

const char kNetworkManagerServiceName[] = "org.freedesktop.NetworkManager";
const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
const char kNetworkManagerInterface[] = "org.freedesktop.NetworkManager";

// From http://projects.gnome.org/NetworkManager/developers/spec.html
enum { NM_DEVICE_TYPE_WIFI = 2 };

// Wifi API binding to NetworkManager, to allow reuse of the polling behavior
// defined in WifiDataProviderCommon.
// TODO(joth): NetworkManager also allows for notification based handling,
// however this will require reworking of the threading code to run a GLib
// event loop (GMainLoop).
class NetworkManagerWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  NetworkManagerWlanApi();

  NetworkManagerWlanApi(const NetworkManagerWlanApi&) = delete;
  NetworkManagerWlanApi& operator=(const NetworkManagerWlanApi&) = delete;

  ~NetworkManagerWlanApi() override;

  // Must be called before any other interface method. Will return false if the
  // NetworkManager session cannot be created (e.g. not present on this distro),
  // in which case no other method may be called.
  bool Init();

  // Similar to Init() but can inject the bus object. Used for testing.
  bool InitWithBus(scoped_refptr<dbus::Bus> bus);

  // WifiDataProviderCommon::WlanApiInterface
  //
  // This function makes blocking D-Bus calls, but it's totally fine as
  // the code runs in "Geolocation" thread, not the browser's UI thread.
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  // Enumerates the list of available network adapter devices known to
  // NetworkManager. Return true on success.
  bool GetAdapterDeviceList(std::vector<dbus::ObjectPath>* device_paths);

  // Given the NetworkManager path to a wireless adapater, dumps the wifi scan
  // results and appends them to |data|. Returns false if a fatal error is
  // encountered such that the data set could not be populated.
  bool GetAccessPointsForAdapter(const dbus::ObjectPath& adapter_path,
                                 WifiData::AccessPointDataSet* data);

  // Internal method used by GetAccessPointsForAdapter(), given a wifi access
  // point proxy retrieves the named property and returns it. Returns nullptr if
  // the property could not be read.
  std::unique_ptr<dbus::Response> GetAccessPointProperty(
      dbus::ObjectProxy* proxy,
      const std::string& property_name);

  scoped_refptr<dbus::Bus> system_bus_;
  raw_ptr<dbus::ObjectProxy> network_manager_proxy_ = nullptr;
};

// Convert a wifi frequency to the corresponding channel. Adapted from
// geolocation/wifilib.cc in googleclient (internal to google).
int frquency_in_khz_to_channel(int frequency_khz) {
  if (frequency_khz >= 2412000 && frequency_khz <= 2472000)  // Channels 1-13.
    return (frequency_khz - 2407000) / 5000;
  if (frequency_khz == 2484000)
    return 14;
  if (frequency_khz > 5000000 && frequency_khz < 6000000)  // .11a bands.
    return (frequency_khz - 5000000) / 5000;
  // Ignore everything else.
  return mojom::kInvalidChannel;
}

NetworkManagerWlanApi::NetworkManagerWlanApi() {}

NetworkManagerWlanApi::~NetworkManagerWlanApi() {
  // Close the connection.
  // This is owned by the system bus, so we need to make sure we're clearing
  // the pointer before its shutdown.
  network_manager_proxy_ = nullptr;
  system_bus_->ShutdownAndBlock();
}

bool NetworkManagerWlanApi::Init() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  options.connection_type = dbus::Bus::PRIVATE;
  return InitWithBus(base::MakeRefCounted<dbus::Bus>(options));
}

bool NetworkManagerWlanApi::InitWithBus(scoped_refptr<dbus::Bus> bus) {
  system_bus_ = bus;
  // system_bus_ will own all object proxies created from the bus.
  network_manager_proxy_ = system_bus_->GetObjectProxy(
      kNetworkManagerServiceName, dbus::ObjectPath(kNetworkManagerPath));
  // Validate the proxy object by checking we can enumerate devices.
  std::vector<dbus::ObjectPath> adapter_paths;
  const bool success = GetAdapterDeviceList(&adapter_paths);
  VLOG(1) << "Init() result:  " << success;
  return success;
}

bool NetworkManagerWlanApi::GetAccessPointData(
    WifiData::AccessPointDataSet* data) {
  std::vector<dbus::ObjectPath> device_paths;
  if (!GetAdapterDeviceList(&device_paths)) {
    LOG(WARNING) << "Could not enumerate access points";
    return false;
  }
  int success_count = 0;
  int fail_count = 0;

  // Iterate the devices, getting APs for each wireless adapter found
  for (const dbus::ObjectPath& device_path : device_paths) {
    VLOG(1) << "Checking device: " << device_path.value();

    dbus::ObjectProxy* device_proxy =
        system_bus_->GetObjectProxy(kNetworkManagerServiceName, device_path);

    dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
    dbus::MessageWriter builder(&method_call);
    builder.AppendString("org.freedesktop.NetworkManager.Device");
    builder.AppendString("DeviceType");
    std::unique_ptr<dbus::Response> response(
        device_proxy
            ->CallMethodAndBlock(&method_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
            .value_or(nullptr));
    if (!response) {
      LOG(WARNING) << "Failed to get the device type for "
                   << device_path.value();
      continue;  // Check the next device.
    }
    dbus::MessageReader reader(response.get());
    uint32_t device_type = 0;
    if (!reader.PopVariantOfUint32(&device_type)) {
      LOG(WARNING) << "Unexpected response for " << device_type << ": "
                   << response->ToString();
      continue;  // Check the next device.
    }
    VLOG(1) << "Device type: " << device_type;

    if (device_type == NM_DEVICE_TYPE_WIFI) {  // Found a wlan adapter
      if (GetAccessPointsForAdapter(device_path, data))
        ++success_count;
      else
        ++fail_count;
    }
  }
  // At least one successful scan overrides any other adapter reporting error.
  return success_count || fail_count == 0;
}

bool NetworkManagerWlanApi::GetAdapterDeviceList(
    std::vector<dbus::ObjectPath>* device_paths) {
  dbus::MethodCall method_call(kNetworkManagerInterface, "GetDevices");
  std::unique_ptr<dbus::Response> response(
      network_manager_proxy_
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));
  if (!response) {
    LOG(WARNING) << "Failed to get the device list";
    return false;
  }

  dbus::MessageReader reader(response.get());
  if (!reader.PopArrayOfObjectPaths(device_paths)) {
    LOG(WARNING) << "Unexpected response: " << response->ToString();
    return false;
  }
  return true;
}

bool NetworkManagerWlanApi::GetAccessPointsForAdapter(
    const dbus::ObjectPath& adapter_path,
    WifiData::AccessPointDataSet* data) {
  // Create a proxy object for this wifi adapter, and ask it to do a scan
  // (or at least, dump its scan results).
  dbus::ObjectProxy* device_proxy =
      system_bus_->GetObjectProxy(kNetworkManagerServiceName, adapter_path);
  dbus::MethodCall method_call("org.freedesktop.NetworkManager.Device.Wireless",
                               "GetAccessPoints");
  std::unique_ptr<dbus::Response> response(
      device_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr));
  if (!response) {
    LOG(WARNING) << "Failed to get access points data for "
                 << adapter_path.value();
    return false;
  }
  dbus::MessageReader reader(response.get());
  std::vector<dbus::ObjectPath> access_point_paths;
  if (!reader.PopArrayOfObjectPaths(&access_point_paths)) {
    LOG(WARNING) << "Unexpected response for " << adapter_path.value() << ": "
                 << response->ToString();
    return false;
  }

  VLOG(1) << "Wireless adapter " << adapter_path.value() << " found "
          << access_point_paths.size() << " access points.";

  for (const dbus::ObjectPath& access_point_path : access_point_paths) {
    VLOG(1) << "Checking access point: " << access_point_path.value();

    dbus::ObjectProxy* access_point_proxy = system_bus_->GetObjectProxy(
        kNetworkManagerServiceName, access_point_path);

    mojom::AccessPointData access_point_data;
    {  // Read the mac address
      std::unique_ptr<dbus::Response> mac_response(
          GetAccessPointProperty(access_point_proxy, "HwAddress"));
      if (!mac_response)
        continue;
      dbus::MessageReader mac_reader(mac_response.get());
      std::string mac;
      if (!mac_reader.PopVariantOfString(&mac)) {
        LOG(WARNING) << "Unexpected response for " << access_point_path.value()
                     << ": " << mac_response->ToString();
        continue;
      }

      base::ReplaceSubstringsAfterOffset(&mac, 0U, ":", std::string_view());
      std::vector<uint8_t> mac_bytes;
      if (!base::HexStringToBytes(mac, &mac_bytes) || mac_bytes.size() != 6) {
        LOG(WARNING) << "Can't parse mac address (found " << mac_bytes.size()
                     << " bytes) so using raw string: " << mac;
        access_point_data.mac_address = mac;
      } else {
        access_point_data.mac_address = MacAddressAsString(&mac_bytes[0]);
      }
    }

    {  // Read signal strength.
      std::unique_ptr<dbus::Response> strength_response(
          GetAccessPointProperty(access_point_proxy, "Strength"));
      if (!strength_response)
        continue;
      dbus::MessageReader strength_reader(strength_response.get());
      uint8_t strength = 0;
      if (!strength_reader.PopVariantOfByte(&strength)) {
        LOG(WARNING) << "Unexpected response for " << access_point_path.value()
                     << ": " << strength_response->ToString();
        continue;
      }
      // Convert strength as a percentage into dBs.
      access_point_data.radio_signal_strength = -100 + strength / 2;
    }

    {  // Read the channel
      std::unique_ptr<dbus::Response> frequency_response(
          GetAccessPointProperty(access_point_proxy, "Frequency"));
      if (!frequency_response)
        continue;
      dbus::MessageReader frequency_reader(frequency_response.get());
      uint32_t frequency = 0;
      if (!frequency_reader.PopVariantOfUint32(&frequency)) {
        LOG(WARNING) << "Unexpected response for " << access_point_path.value()
                     << ": " << frequency_response->ToString();
        continue;
      }

      // NetworkManager returns frequency in MHz.
      access_point_data.channel = frquency_in_khz_to_channel(frequency * 1000);
    }
    VLOG(1) << "Access point data of " << access_point_path.value() << ": "
            << "MAC: " << access_point_data.mac_address << ", "
            << "Strength: " << access_point_data.radio_signal_strength << ", "
            << "Channel: " << access_point_data.channel;

    data->insert(access_point_data);
  }
  return true;
}

std::unique_ptr<dbus::Response> NetworkManagerWlanApi::GetAccessPointProperty(
    dbus::ObjectProxy* access_point_proxy,
    const std::string& property_name) {
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
  dbus::MessageWriter builder(&method_call);
  builder.AppendString("org.freedesktop.NetworkManager.AccessPoint");
  builder.AppendString(property_name);
  std::unique_ptr<dbus::Response> response =
      access_point_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  if (!response) {
    LOG(WARNING) << "Failed to get property for " << property_name;
  }
  return response;
}

}  // namespace

// static
WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderLinux();
}

WifiDataProviderLinux::WifiDataProviderLinux() {}

WifiDataProviderLinux::~WifiDataProviderLinux() {}

std::unique_ptr<WifiDataProviderCommon::WlanApiInterface>
WifiDataProviderLinux::CreateWlanApi() {
  auto wlan_api = std::make_unique<NetworkManagerWlanApi>();
  if (wlan_api->Init())
    return std::move(wlan_api);
  return nullptr;
}

std::unique_ptr<WifiPollingPolicy>
WifiDataProviderLinux::CreatePollingPolicy() {
  return std::make_unique<GenericWifiPollingPolicy<
      kDefaultPollingIntervalMilliseconds, kNoChangePollingIntervalMilliseconds,
      kTwoNoChangePollingIntervalMilliseconds,
      kNoWifiPollingIntervalMilliseconds>>();
}

std::unique_ptr<WifiDataProviderCommon::WlanApiInterface>
WifiDataProviderLinux::CreateWlanApiForTesting(scoped_refptr<dbus::Bus> bus) {
  auto wlan_api = std::make_unique<NetworkManagerWlanApi>();
  if (wlan_api->InitWithBus(bus))
    return std::move(wlan_api);
  return nullptr;
}

}  // namespace device
