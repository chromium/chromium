// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for suitable for typical linux distributions.
// Currently, only the NetworkManager API is used, accessed via D-Bus.

#include "services/device/geolocation/wifi_data_provider_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "services/device/geolocation/wifi_data.h"
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

// Convert a wifi frequency to the corresponding channel. Adapted from
// geolocation/wifilib.cc in googleclient (internal to google).
int FrequencyInKhzToChannel(int frequency_khz) {
  if (frequency_khz >= 2412000 && frequency_khz <= 2472000) {  // Channels 1-13.
    return (frequency_khz - 2407000) / 5000;
  }
  if (frequency_khz == 2484000) {
    return 14;
  }
  if (frequency_khz > 5000000 && frequency_khz < 6000000) {  // .11a bands.
    return (frequency_khz - 5000000) / 5000;
  }
  // Ignore everything else.
  return mojom::kInvalidChannel;
}

// State for an asynchronous GetAccessPointData call. This is ref-counted
// to allow it to be safely passed through the asynchronous call chain.
class GetAccessPointDataState
    : public base::RefCounted<GetAccessPointDataState> {
 public:
  explicit GetAccessPointDataState(
      base::OnceCallback<void(std::unique_ptr<WifiData::AccessPointDataSet>)>
          final_callback)
      : data_(std::make_unique<WifiData::AccessPointDataSet>()),
        callback_(std::move(final_callback)) {}

  // Called when processing for an adapter is finished.
  void OnAdapterFinished(bool success) {
    if (success) {
      success_count_++;
    } else {
      fail_count_++;
    }
  }

  WifiData::AccessPointDataSet* data() { return data_.get(); }

 private:
  friend class base::RefCounted<GetAccessPointDataState>;

  ~GetAccessPointDataState() {
    if (callback_) {
      if (success_count_ > 0 || fail_count_ == 0) {
        std::move(callback_).Run(std::move(data_));
      } else {
        std::move(callback_).Run(nullptr);
      }
    }
  }

  std::unique_ptr<WifiData::AccessPointDataSet> data_;
  base::OnceCallback<void(std::unique_ptr<WifiData::AccessPointDataSet>)>
      callback_;
  int success_count_ = 0;
  int fail_count_ = 0;
};

// State for fetching info for a single access point.
struct AccessPointInfoState {
  AccessPointInfoState(scoped_refptr<GetAccessPointDataState> parent_state,
                       base::OnceClosure finished_closure,
                       const dbus::ObjectPath& path,
                       dbus::ObjectProxy* proxy)
      : state(std::move(parent_state)),
        closure(std::move(finished_closure)),
        access_point_path(path),
        access_point_proxy(proxy) {}

  ~AccessPointInfoState() { std::move(closure).Run(); }

  // Adds the completed `access_point_data` to the main dataset.
  void AddDataToParent() { state->data()->insert(access_point_data); }

  scoped_refptr<GetAccessPointDataState> state;
  base::OnceClosure closure;
  const dbus::ObjectPath access_point_path;
  raw_ptr<dbus::ObjectProxy> access_point_proxy = nullptr;
  mojom::AccessPointData access_point_data;
};

// Wifi API binding to NetworkManager, to allow reuse of the polling behavior
// defined in WifiDataProviderCommon.
class NetworkManagerWlanApiImpl {
 public:
  explicit NetworkManagerWlanApiImpl(scoped_refptr<dbus::Bus> bus)
      : system_bus_(std::move(bus)) {
    CHECK(system_bus_);
    network_manager_proxy_ = system_bus_->GetObjectProxy(
        kNetworkManagerServiceName, dbus::ObjectPath(kNetworkManagerPath));
  }

  NetworkManagerWlanApiImpl(const NetworkManagerWlanApiImpl&) = delete;
  NetworkManagerWlanApiImpl& operator=(const NetworkManagerWlanApiImpl&) =
      delete;

  ~NetworkManagerWlanApiImpl() = default;

  void GetAccessPointData(
      base::OnceCallback<void(std::unique_ptr<WifiData::AccessPointDataSet>)>
          callback) {
    auto state =
        base::MakeRefCounted<GetAccessPointDataState>(std::move(callback));

    dbus::MethodCall method_call(kNetworkManagerInterface, "GetDevices");
    network_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&NetworkManagerWlanApiImpl::OnGetDevicesResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(state)));
  }

 private:
  void OnGetDevicesResponse(scoped_refptr<GetAccessPointDataState> state,
                            dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get the device list";
      state->OnAdapterFinished(false);
      return;
    }

    dbus::MessageReader reader(response);
    std::vector<dbus::ObjectPath> device_paths;
    if (!reader.PopArrayOfObjectPaths(&device_paths)) {
      LOG(WARNING) << "Unexpected response: " << response->ToString();
      state->OnAdapterFinished(false);
      return;
    }

    for (const dbus::ObjectPath& device_path : device_paths) {
      VLOG(1) << "Checking device: " << device_path.value();
      dbus::ObjectProxy* device_proxy =
          system_bus_->GetObjectProxy(kNetworkManagerServiceName, device_path);
      dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
      dbus::MessageWriter writer(&method_call);
      writer.AppendString("org.freedesktop.NetworkManager.Device");
      writer.AppendString("DeviceType");

      device_proxy->CallMethod(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(&NetworkManagerWlanApiImpl::OnGetDeviceTypeResponse,
                         weak_ptr_factory_.GetWeakPtr(), state, device_path));
    }
  }

  void OnGetDeviceTypeResponse(scoped_refptr<GetAccessPointDataState> state,
                               const dbus::ObjectPath& device_path,
                               dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get the device type for "
                   << device_path.value();
      return;
    }
    dbus::MessageReader reader(response);
    uint32_t device_type = 0;
    if (!reader.PopVariantOfUint32(&device_type)) {
      LOG(WARNING) << "Unexpected response for device type: "
                   << response->ToString();
      return;
    }

    if (device_type == NM_DEVICE_TYPE_WIFI) {
      GetAccessPointsForAdapter(state, device_path);
    }
  }

  void GetAccessPointsForAdapter(scoped_refptr<GetAccessPointDataState> state,
                                 const dbus::ObjectPath& adapter_path) {
    dbus::ObjectProxy* device_proxy =
        system_bus_->GetObjectProxy(kNetworkManagerServiceName, adapter_path);
    dbus::MethodCall method_call(
        "org.freedesktop.NetworkManager.Device.Wireless", "GetAccessPoints");
    device_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&NetworkManagerWlanApiImpl::OnGetAccessPointsForAdapter,
                       weak_ptr_factory_.GetWeakPtr(), state, adapter_path));
  }

  void OnGetAccessPointsForAdapter(scoped_refptr<GetAccessPointDataState> state,
                                   const dbus::ObjectPath& adapter_path,
                                   dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get access points data for "
                   << adapter_path.value();
      state->OnAdapterFinished(false);
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<dbus::ObjectPath> access_point_paths;
    if (!reader.PopArrayOfObjectPaths(&access_point_paths)) {
      LOG(WARNING) << "Unexpected response for " << adapter_path.value() << ": "
                   << response->ToString();
      state->OnAdapterFinished(false);
      return;
    }

    VLOG(1) << "Wireless adapter " << adapter_path.value() << " found "
            << access_point_paths.size() << " access points.";

    if (access_point_paths.empty()) {
      state->OnAdapterFinished(true);
      return;
    }

    base::RepeatingClosure barrier = base::BarrierClosure(
        access_point_paths.size(),
        base::BindOnce(&GetAccessPointDataState::OnAdapterFinished, state,
                       true));

    for (const dbus::ObjectPath& access_point_path : access_point_paths) {
      VLOG(1) << "Checking access point: " << access_point_path.value();
      dbus::ObjectProxy* access_point_proxy = system_bus_->GetObjectProxy(
          kNetworkManagerServiceName, access_point_path);

      auto ap_state = std::make_unique<AccessPointInfoState>(
          state, barrier, access_point_path, access_point_proxy);

      GetAccessPointProperty(
          access_point_proxy, "HwAddress",
          base::BindOnce(&NetworkManagerWlanApiImpl::OnGetHwAddress,
                         weak_ptr_factory_.GetWeakPtr(), std::move(ap_state)));
    }
  }

  void GetAccessPointProperty(
      dbus::ObjectProxy* access_point_proxy,
      const std::string& property_name,
      base::OnceCallback<void(dbus::Response*)> callback) {
    dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.freedesktop.NetworkManager.AccessPoint");
    writer.AppendString(property_name);
    access_point_proxy->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   std::move(callback));
  }

  void OnGetHwAddress(std::unique_ptr<AccessPointInfoState> ap_state,
                      dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get HwAddress for "
                   << ap_state->access_point_path.value();
      // State destructor will run the closure.
      return;
    }

    dbus::MessageReader reader(response);
    std::string mac;
    if (!reader.PopVariantOfString(&mac)) {
      return;
    }

    base::ReplaceSubstringsAfterOffset(&mac, 0U, ":", std::string_view());
    std::vector<uint8_t> mac_bytes;
    if (!base::HexStringToBytes(mac, &mac_bytes) || mac_bytes.size() != 6) {
      LOG(WARNING) << "Can't parse mac address (found " << mac_bytes.size()
                   << " bytes) so using raw string: " << mac;
      ap_state->access_point_data.mac_address = mac;
    } else {
      ap_state->access_point_data.mac_address =
          MacAddressAsString(base::span<const uint8_t, 6>(mac_bytes));
    }

    auto* access_point_proxy = ap_state->access_point_proxy.get();
    GetAccessPointProperty(
        access_point_proxy, "Strength",
        base::BindOnce(&NetworkManagerWlanApiImpl::OnGetStrength,
                       weak_ptr_factory_.GetWeakPtr(), std::move(ap_state)));
  }

  void OnGetStrength(std::unique_ptr<AccessPointInfoState> ap_state,
                     dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get Strength for "
                   << ap_state->access_point_path.value();
      return;
    }

    dbus::MessageReader reader(response);
    uint8_t strength = 0;
    if (!reader.PopVariantOfByte(&strength)) {
      return;
    }

    // Convert strength as a percentage into dBs.
    ap_state->access_point_data.radio_signal_strength = -100 + strength / 2;

    auto* access_point_proxy = ap_state->access_point_proxy.get();
    GetAccessPointProperty(
        access_point_proxy, "Frequency",
        base::BindOnce(&NetworkManagerWlanApiImpl::OnGetFrequency,
                       weak_ptr_factory_.GetWeakPtr(), std::move(ap_state)));
  }

  void OnGetFrequency(std::unique_ptr<AccessPointInfoState> ap_state,
                      dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Failed to get Frequency for "
                   << ap_state->access_point_path.value();
      return;
    }

    dbus::MessageReader reader(response);
    uint32_t frequency = 0;
    if (!reader.PopVariantOfUint32(&frequency)) {
      return;
    }

    // NetworkManager returns frequency in MHz.
    ap_state->access_point_data.channel =
        FrequencyInKhzToChannel(frequency * 1000);
    VLOG(1) << "Access point data of " << ap_state->access_point_path.value()
            << ": "
            << "MAC: " << ap_state->access_point_data.mac_address << ", "
            << "Strength: " << ap_state->access_point_data.radio_signal_strength
            << ", "
            << "Channel: " << ap_state->access_point_data.channel;

    ap_state->AddDataToParent();
  }

  scoped_refptr<dbus::Bus> system_bus_;
  raw_ptr<dbus::ObjectProxy> network_manager_proxy_ = nullptr;
  base::WeakPtrFactory<NetworkManagerWlanApiImpl> weak_ptr_factory_{this};
};

class NetworkManagerWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  explicit NetworkManagerWlanApi(scoped_refptr<dbus::Bus> bus)
      : impl_(bus->GetOriginTaskRunner(), bus) {}

  NetworkManagerWlanApi(const NetworkManagerWlanApi&) = delete;
  NetworkManagerWlanApi& operator=(const NetworkManagerWlanApi&) = delete;

  ~NetworkManagerWlanApi() override = default;

  // WifiDataProviderCommon::WlanApiInterface
  void GetAccessPointData(
      base::OnceCallback<void(std::unique_ptr<WifiData::AccessPointDataSet>)>
          callback) override {
    impl_.AsyncCall(&NetworkManagerWlanApiImpl::GetAccessPointData)
        .WithArgs(base::BindPostTaskToCurrentDefault(std::move(callback)));
  }

 private:
  base::SequenceBound<NetworkManagerWlanApiImpl> impl_;
};

}  // namespace

// static
WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderLinux();
}

WifiDataProviderLinux::WifiDataProviderLinux() = default;

WifiDataProviderLinux::~WifiDataProviderLinux() = default;

std::unique_ptr<WifiDataProviderCommon::WlanApiInterface>
WifiDataProviderLinux::CreateWlanApi() {
  return std::make_unique<NetworkManagerWlanApi>(
      dbus_thread_linux::GetSharedSystemBus());
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
  return std::make_unique<NetworkManagerWlanApi>(bus);
}

}  // namespace device
