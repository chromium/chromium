// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/bluetooth_system.mojom-test-utils.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

namespace {

// Adapter object paths
constexpr const char kDefaultAdapterObjectPathStr[] = "fake/hci0";
constexpr const char kAlternateAdapterObjectPathStr[] = "fake/hci1";

// Device object paths
constexpr const char kDefaultDeviceObjectPathStr[] =
    "fake/hci0/dev_00_11_22_AA_BB_CC";

// Device addresses
constexpr const char kDefaultDeviceAddressStr[] = "00:11:22:AA:BB:CC";
constexpr const char kAlternateDeviceAddressStr[] = "AA:BB:CC:DD:EE:FF";

constexpr const std::array<uint8_t, 6> kDefaultDeviceAddressArray = {
    0x00, 0x11, 0x22, 0xAA, 0xBB, 0xCC};

bool GetValueAndReset(absl::optional<bool>* opt) {
  absl::optional<bool> tmp;
  tmp.swap(*opt);
  return tmp.value();
}

struct FakeDeviceOptions {
  explicit FakeDeviceOptions(
      const std::string& object_path = kDefaultDeviceObjectPathStr)
      : object_path(object_path) {}

  dbus::ObjectPath object_path;

  std::string address{kDefaultDeviceAddressStr};
  dbus::ObjectPath adapter_object_path{kDefaultAdapterObjectPathStr};
  absl::optional<std::string> name;
  bool paired = false;
  bool connected = false;
};

// Exposes high-level methods to simulate Bluetooth events e.g. a new adapter
// was added, adapter power state changed, etc.
//
// As opposed to FakeBluetoothAdapterClient, the other fake implementation of
// BluetoothAdapterClient, this class does not have any built-in behavior
// e.g. it won't start triggering device discovery events when StartDiscovery is
// called. It's up to its users to call the relevant Simulate*() method to
// trigger each event.
class DEVICE_BLUETOOTH_EXPORT TestBluetoothAdapterClient
    : public bluez::BluetoothAdapterClient {
 public:
  struct Properties : public bluez::BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothAdapterClient::Properties(
              nullptr, /* object_proxy */
              bluetooth_adapter::kBluetoothAdapterInterface,
              callback) {}
    ~Properties() override = default;

    void ResetCallCount() {
      set_powered_call_count_ = 0;
      last_set_powered_value_.reset();
    }

    void SetNextSetPoweredResponse(bool next_response) {
      DCHECK(!next_set_powered_response_);
      next_set_powered_response_ = next_response;
    }

    bool GetLastSetPoweredValue() { return last_set_powered_value_.value(); }

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override {
      DVLOG(1) << "Get " << property->name();
      NOTIMPLEMENTED();
    }

    void GetAll() override {
      DVLOG(1) << "GetAll";
      NOTIMPLEMENTED();
    }

    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override {
      DVLOG(1) << "Set " << property->name();
      if (property->name() == powered.name()) {
        ++set_powered_call_count_;
        last_set_powered_value_ = powered.GetSetValueForTesting();
        if (next_set_powered_response_) {
          std::move(callback).Run(
              GetValueAndReset(&next_set_powered_response_));
          return;
        }
        set_powered_callbacks_.push_back(std::move(callback));
      } else {
        NOTIMPLEMENTED();
      }
    }

    size_t set_powered_call_count_ = 0;
    absl::optional<bool> next_set_powered_response_;
    absl::optional<bool> last_set_powered_value_;

    // Saved `Set('powered')` callbacks. If there is no next response set for a
    // `Set()` call, then the callback is saved here. TestBluetoothAdapterClient
    // runs all these callbacks after the adapter is removed.
    std::deque<base::OnceCallback<void(bool)>> set_powered_callbacks_;
  };

  TestBluetoothAdapterClient() = default;
  ~TestBluetoothAdapterClient() override = default;

  void ResetCallCount() {
    for (auto& path_to_properties : adapter_object_paths_to_properties_) {
      path_to_properties.second->ResetCallCount();
    }

    for (auto& path_to_call_counts : adapter_object_paths_to_call_counts_) {
      path_to_call_counts.second = CallCounts();
    }
  }

  // Low level methods to simulate events and operations. All actions are
  // performed for `kDefaultAdapterObjectPathStr` unless a different one is
  // specified.

  // Simulates a new adapter with |object_path_str|. Its properties are empty,
  // 0, or false.
  void SimulateAdapterAdded(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);

    auto [it, was_inserted] = adapter_object_paths_to_properties_.emplace(
        object_path, std::make_unique<Properties>(base::BindRepeating(
                         &TestBluetoothAdapterClient::OnPropertyChanged,
                         base::Unretained(this), object_path)));
    DCHECK(was_inserted);

    DCHECK(!base::Contains(adapter_object_paths_to_call_counts_, object_path));
    adapter_object_paths_to_call_counts_[object_path];

    DCHECK(
        !base::Contains(adapter_object_paths_to_next_responses_, object_path));
    adapter_object_paths_to_next_responses_[object_path];

    GetProperties(object_path)->powered.ReplaceValue(false);
    GetProperties(object_path)->discovering.ReplaceValue(false);

    for (auto& observer : observers_)
      observer.AdapterAdded(object_path);
  }

  // Simulates the adapter at |object_path_str| being removed.
  void SimulateAdapterRemoved(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);

    // Properties are set to empty, 0, or false right before AdapterRemoved is
    // called.
    GetProperties(object_path)->powered.ReplaceValue(false);
    GetProperties(object_path)->discovering.ReplaceValue(false);

    // When BlueZ calls into AdapterRemoved, the adapter is still exposed
    // through GetAdapters() and its properties are still accessible.
    for (auto& observer : observers_)
      observer.AdapterRemoved(object_path);

    auto properties =
        std::move(adapter_object_paths_to_properties_[object_path]);
    size_t removed = adapter_object_paths_to_properties_.erase(object_path);
    DCHECK_EQ(1u, removed);
    removed = adapter_object_paths_to_call_counts_.erase(object_path);
    DCHECK_EQ(1u, removed);
    removed = adapter_object_paths_to_next_responses_.erase(object_path);
    DCHECK_EQ(1u, removed);

    // After the adapter is removed, any pending Set calls get run with `false`.
    for (auto& set_powered_callback : properties->set_powered_callbacks_) {
      std::move(set_powered_callback).Run(false);
    }
  }

  // Simulates adapter at |object_path_str| changing its powered state to
  // |powered|.
  void SimulateAdapterPowerStateChanged(
      bool powered,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    auto* properties = GetProperties(dbus::ObjectPath(object_path_str));
    properties->powered.ReplaceValue(powered);

    // After the "powered" property changes to false, BlueZ emits a property
    // changed event for "discovering" as well, even if the property was false
    // already.
    if (!powered)
      properties->discovering.ReplaceValue(false);
  }

  void SetNextSetPoweredResponse(
      bool response,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->SetNextSetPoweredResponse(response);
  }

  size_t GetSetPoweredCallCount(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    auto it = adapter_object_paths_to_properties_.find(
        dbus::ObjectPath(object_path_str));
    DCHECK(it != adapter_object_paths_to_properties_.end());

    return it->second->set_powered_call_count_;
  }

  bool GetLastSetPoweredValue(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    return GetProperties(dbus::ObjectPath(object_path_str))
        ->GetLastSetPoweredValue();
  }

  void SimulateSetPoweredCompleted(
      bool success,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    auto& callbacks = GetProperties(dbus::ObjectPath(object_path_str))
                          ->set_powered_callbacks_;
    auto callback = std::move(callbacks.front());
    callbacks.pop_front();

    std::move(callback).Run(success);
  }

  // Simulates adapter at |object_path_str| changing its discovering state to
  // |powered|.
  void SimulateAdapterDiscoveringStateChanged(
      bool discovering,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->discovering.ReplaceValue(discovering);
  }

  void SetNextStartDiscoveryResponse(
      bool response,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);

    auto& next_response =
        adapter_object_paths_to_next_responses_[object_path].start_discovery;
    DCHECK(!next_response.has_value());
    next_response = response;
  }

  size_t GetStartDiscoveryCallCount(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);
    return adapter_object_paths_to_call_counts_[object_path].start_discovery;
  }

  void SetNextStopDiscoveryResponse(
      bool response,
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);
    auto& next_response =
        adapter_object_paths_to_next_responses_[object_path].stop_discovery;
    DCHECK(!next_response.has_value());
    next_response = response;
  }

  size_t GetStopDiscoveryCallCount(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    dbus::ObjectPath object_path(object_path_str);
    return adapter_object_paths_to_call_counts_[object_path].stop_discovery;
  }

  // Helper methods to perform multiple common operations.

  // Simultes adding an adapter and it changing its state to powered On.
  void SimulatePoweredOnAdapter(
      const std::string& object_path_str = kDefaultAdapterObjectPathStr) {
    SimulateAdapterAdded(object_path_str);
    SimulateAdapterPowerStateChanged(true, object_path_str);
  }

  // BluetoothAdapterClient:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  std::vector<dbus::ObjectPath> GetAdapters() override {
    std::vector<dbus::ObjectPath> object_paths;
    for (const auto& object_path_to_property :
         adapter_object_paths_to_properties_) {
      object_paths.push_back(object_path_to_property.first);
    }
    return object_paths;
  }

  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    auto it = adapter_object_paths_to_properties_.find(object_path);
    if (it == adapter_object_paths_to_properties_.end())
      return nullptr;
    return it->second.get();
  }

  void StartDiscovery(const dbus::ObjectPath& object_path,
                      ResponseCallback callback) override {
    DCHECK(base::Contains(adapter_object_paths_to_call_counts_, object_path));
    ++adapter_object_paths_to_call_counts_[object_path].start_discovery;

    DCHECK(
        base::Contains(adapter_object_paths_to_next_responses_, object_path));

    absl::optional<bool> response;
    response.swap(
        adapter_object_paths_to_next_responses_[object_path].start_discovery);

    if (!response.value()) {
      std::move(callback).Run(Error(kUnknownAdapterError, "Unknown error"));
      return;
    }
    std::move(callback).Run(absl::nullopt);
  }

  void StopDiscovery(const dbus::ObjectPath& object_path,
                     ResponseCallback callback) override {
    DCHECK(base::Contains(adapter_object_paths_to_call_counts_, object_path));
    ++adapter_object_paths_to_call_counts_[object_path].stop_discovery;

    DCHECK(
        base::Contains(adapter_object_paths_to_next_responses_, object_path));

    absl::optional<bool> response;
    response.swap(
        adapter_object_paths_to_next_responses_[object_path].stop_discovery);

    if (!response.value()) {
      std::move(callback).Run(Error(kUnknownAdapterError, "Unknown error"));
      return;
    }
    std::move(callback).Run(absl::nullopt);
  }

  void RemoveDevice(const dbus::ObjectPath& object_path,
                    const dbus::ObjectPath& device_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                          const DiscoveryFilter& discovery_filter,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void CreateServiceRecord(const dbus::ObjectPath& object_path,
                           const bluez::BluetoothServiceRecordBlueZ& record,
                           ServiceRecordCallback callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                           uint32_t handle,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void ConnectDevice(const dbus::ObjectPath& object_path,
                     const std::string& address,
                     const absl::optional<AddressType>& address_type,
                     ConnectDeviceCallback callback,
                     ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

 private:
  // Keeps track of how many times methods have been called.
  struct CallCounts {
    size_t start_discovery;
    size_t stop_discovery;
  };

  // Keeps track of the responses to send when a method is called.
  struct NextResponses {
    NextResponses() = default;
    ~NextResponses() = default;

    absl::optional<bool> start_discovery;
    absl::optional<bool> stop_discovery;
  };

  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_) {
      observer.AdapterPropertyChanged(object_path, property_name);
    }
  }

  using ObjectPathToProperties =
      std::map<dbus::ObjectPath, std::unique_ptr<Properties>>;
  ObjectPathToProperties adapter_object_paths_to_properties_;

  // Keeps track of how many times each method has been called for a specific
  // adapter ObjectPath.
  std::map<dbus::ObjectPath, CallCounts> adapter_object_paths_to_call_counts_;

  // Keeps track of the next responses to send when methods are called for a
  // specific adapter ObjectPath.
  std::map<dbus::ObjectPath, NextResponses>
      adapter_object_paths_to_next_responses_;

  base::ObserverList<Observer>::Unchecked observers_;
};

// Exposes high-level methods to simulate Bluetooth device events e.g. a new
// device was added, device connected, etc.
//
// As opposed to FakeBluetoothDeviceClient, the other fake implementation of
// BluetoothDeviceClient, this class does not have any built-in behavior
// e.g. it won't start triggering device discovery events when StartDiscovery is
// called. It's up to its users to call the relevant Simulate*() method to
// trigger each event.
class DEVICE_BLUETOOTH_EXPORT TestBluetoothDeviceClient
    : public bluez::BluetoothDeviceClient {
 public:
  struct Properties : public bluez::BluetoothDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothDeviceClient::Properties(
              nullptr /* object_proxy */,
              bluetooth_device::kBluetoothDeviceInterface,
              callback) {}
    ~Properties() override = default;

    // dbus::PropertySet
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override {
      DVLOG(1) << "Get " << property->name();
      NOTIMPLEMENTED();
    }

    void GetAll() override {
      DVLOG(1) << "GetAll";
      NOTIMPLEMENTED();
    }

    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override {
      DVLOG(1) << "Set " << property->name();
      NOTIMPLEMENTED();
    }
  };

  TestBluetoothDeviceClient() = default;
  ~TestBluetoothDeviceClient() override = default;

  void SimulateDeviceAdded(const FakeDeviceOptions& options) {
    auto [it, was_inserted] = device_object_paths_to_properties_.emplace(
        options.object_path,
        std::make_unique<Properties>(
            base::BindLambdaForTesting([&](const std::string& property_name) {
              for (auto& observer : observers_)
                observer.DevicePropertyChanged(options.object_path,
                                               property_name);
            })));

    DCHECK(was_inserted);

    auto* properties = GetProperties(options.object_path);
    properties->address.ReplaceValue(options.address);

    if (options.name) {
      properties->name.set_valid(true);
      properties->name.ReplaceValue(options.name.value());
    }

    properties->paired.ReplaceValue(options.paired);
    properties->connected.ReplaceValue(options.connected);
    properties->adapter.ReplaceValue(options.adapter_object_path);

    for (auto& observer : observers_)
      observer.DeviceAdded(options.object_path);
  }

  // bluez::BluetoothDeviceClient
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) override {
    std::vector<dbus::ObjectPath> devices;
    for (const auto& path_and_properties : device_object_paths_to_properties_) {
      if (path_and_properties.second->adapter.value() == adapter_path)
        devices.push_back(path_and_properties.first);
    }
    return devices;
  }

  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    auto it = device_object_paths_to_properties_.find(object_path);
    if (it == device_object_paths_to_properties_.end())
      return nullptr;
    return it->second.get();
  }

  void Connect(const dbus::ObjectPath& object_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void ConnectClassic(const dbus::ObjectPath& object_path,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void ConnectLE(const dbus::ObjectPath& object_path,
                 base::OnceClosure callback,
                 ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void Disconnect(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void DisconnectLE(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void ConnectProfile(const dbus::ObjectPath& object_path,
                      const std::string& uuid,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void DisconnectProfile(const dbus::ObjectPath& object_path,
                         const std::string& uuid,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void Pair(const dbus::ObjectPath& object_path,
            base::OnceClosure callback,
            ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void CancelPairing(const dbus::ObjectPath& object_path,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void GetConnInfo(const dbus::ObjectPath& object_path,
                   ConnInfoCallback callback,
                   ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void SetLEConnectionParameters(const dbus::ObjectPath& object_path,
                                 const ConnectionParameters& conn_params,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void GetServiceRecords(const dbus::ObjectPath& object_path,
                         ServiceRecordsCallback callback,
                         ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void ExecuteWrite(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void AbortWrite(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

 private:
  using ObjectPathToProperties =
      std::map<dbus::ObjectPath, std::unique_ptr<Properties>>;
  ObjectPathToProperties device_object_paths_to_properties_;

  base::ObserverList<Observer>::Unchecked observers_;
};

// Exposes high-level methods to retrieve Bluetooth device admin policy info.
class DEVICE_BLUETOOTH_EXPORT TestBluetoothAdminPolicyClient
    : public bluez::BluetoothAdminPolicyClient {
 public:
  struct Properties : public bluez::BluetoothAdminPolicyClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothAdminPolicyClient::Properties(
              /*object_proxy=*/nullptr,
              bluetooth_admin_policy::kBluetoothAdminPolicyStatusInterface,
              callback) {}
    ~Properties() override = default;

    // dbus::PropertySet
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override {
      NOTIMPLEMENTED() << "Get " << property->name();
    }

    void GetAll() override { NOTIMPLEMENTED() << "GetAll"; }

    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override {
      NOTIMPLEMENTED() << "Set " << property->name();
    }
  };

  TestBluetoothAdminPolicyClient() = default;
  ~TestBluetoothAdminPolicyClient() override = default;

  void SimulateAdminPolicyAdded(const dbus::ObjectPath& object_path,
                                bool is_blocked_by_policy) {
    auto [it, was_inserted] = device_object_paths_to_properties_.emplace(
        object_path,
        std::make_unique<Properties>(
            base::BindLambdaForTesting([&](const std::string& property_name) {
              for (auto& observer : observers_)
                observer.AdminPolicyPropertyChanged(object_path, property_name);
            })));

    DCHECK(was_inserted);

    auto* properties = GetProperties(object_path);
    properties->is_blocked_by_policy.ReplaceValue(is_blocked_by_policy);
    properties->is_blocked_by_policy.set_valid(true);

    for (auto& observer : observers_)
      observer.AdminPolicyAdded(object_path);
  }

  // bluez::BluetoothAdminPolicyClient
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    auto it = device_object_paths_to_properties_.find(object_path);
    if (it == device_object_paths_to_properties_.end())
      return nullptr;
    return it->second.get();
  }

  void SetServiceAllowList(const dbus::ObjectPath& object_path,
                           const UUIDList& service_uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED() << "SetServiceAllowList";
  }

 private:
  using ObjectPathToProperties =
      std::map<dbus::ObjectPath, std::unique_ptr<Properties>>;
  ObjectPathToProperties device_object_paths_to_properties_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace

class BluetoothSystemTest : public DeviceServiceTestBase,
                            public mojom::BluetoothSystemClient {
 public:
  BluetoothSystemTest() = default;

  BluetoothSystemTest(const BluetoothSystemTest&) = delete;
  BluetoothSystemTest& operator=(const BluetoothSystemTest&) = delete;

  ~BluetoothSystemTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    device_service()->BindBluetoothSystemFactory(
        system_factory_.BindNewPipeAndPassReceiver());

    auto test_bluetooth_adapter_client =
        std::make_unique<TestBluetoothAdapterClient>();
    test_bluetooth_adapter_client_ = test_bluetooth_adapter_client.get();
    auto test_bluetooth_admin_policy_client =
        std::make_unique<TestBluetoothAdminPolicyClient>();
    test_bluetooth_admin_policy_client_ =
        test_bluetooth_admin_policy_client.get();
    auto test_bluetooth_device_client =
        std::make_unique<TestBluetoothDeviceClient>();
    test_bluetooth_device_client_ = test_bluetooth_device_client.get();

    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetAlternateBluetoothAdapterClient(
        std::move(test_bluetooth_adapter_client));
    dbus_setter->SetAlternateBluetoothAdminPolicyClient(
        std::move(test_bluetooth_admin_policy_client));
    dbus_setter->SetAlternateBluetoothDeviceClient(
        std::move(test_bluetooth_device_client));
  }

  // Helper methods to avoid AsyncWaiter boilerplate.
  mojom::BluetoothSystem::State GetStateAndWait(
      mojom::BluetoothSystem* system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    mojom::BluetoothSystem::State state;
    async_waiter.GetState(&state);

    return state;
  }

  mojom::BluetoothSystem::SetPoweredResult SetPoweredAndWait(
      mojom::BluetoothSystem* system,
      bool powered) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    mojom::BluetoothSystem::SetPoweredResult result;
    async_waiter.SetPowered(powered, &result);

    return result;
  }

  mojom::BluetoothSystem::ScanState GetScanStateAndWait(
      mojom::BluetoothSystem* system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    mojom::BluetoothSystem::ScanState scan_state;
    async_waiter.GetScanState(&scan_state);

    return scan_state;
  }

  mojom::BluetoothSystem::StartScanResult StartScanAndWait(
      mojom::BluetoothSystem* system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    mojom::BluetoothSystem::StartScanResult result;
    async_waiter.StartScan(&result);

    return result;
  }

  mojom::BluetoothSystem::StopScanResult StopScanAndWait(
      mojom::BluetoothSystem* system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    mojom::BluetoothSystem::StopScanResult result;
    async_waiter.StopScan(&result);

    return result;
  }

  std::vector<mojom::BluetoothDeviceInfoPtr> GetAvailableDevicesAndWait(
      mojom::BluetoothSystem* system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system);

    std::vector<mojom::BluetoothDeviceInfoPtr> devices;
    async_waiter.GetAvailableDevices(&devices);

    return devices;
  }

  // mojom::BluetoothSystemClient
  void OnStateChanged(mojom::BluetoothSystem::State state) override {
    on_state_changed_states_.push_back(state);
  }

  void OnScanStateChanged(mojom::BluetoothSystem::ScanState state) override {
    on_scan_state_changed_states_.push_back(state);
  }

 protected:
  mojo::Remote<mojom::BluetoothSystem> CreateBluetoothSystem() {
    mojo::Remote<mojom::BluetoothSystem> system;
    system_factory_->Create(system.BindNewPipeAndPassReceiver(),
                            system_client_receiver_.BindNewPipeAndPassRemote());
    return system;
  }

  void ResetResults() {
    on_state_changed_states_.clear();
    on_scan_state_changed_states_.clear();
  }

  // Saves the states passed to OnStateChanged.
  using StateVector = std::vector<mojom::BluetoothSystem::State>;
  StateVector on_state_changed_states_;

  // Saves the states passed to OnScanStateChanged.
  using ScanStateVector = std::vector<mojom::BluetoothSystem::ScanState>;
  ScanStateVector on_scan_state_changed_states_;

  mojo::Remote<mojom::BluetoothSystemFactory> system_factory_;

  TestBluetoothAdapterClient* test_bluetooth_adapter_client_;
  TestBluetoothAdminPolicyClient* test_bluetooth_admin_policy_client_;
  TestBluetoothDeviceClient* test_bluetooth_device_client_;

  mojo::Receiver<mojom::BluetoothSystemClient> system_client_receiver_{this};
};

// Tests that the Create method for BluetoothSystemFactory works.
TEST_F(BluetoothSystemTest, FactoryCreate) {
  mojo::Remote<mojom::BluetoothSystem> system;
  mojo::Receiver<mojom::BluetoothSystemClient> client_receiver(this);

  EXPECT_FALSE(system.is_bound());

  system_factory_->Create(system.BindNewPipeAndPassReceiver(),
                          client_receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  system.FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(system.is_bound());
}

// Tests that the state is 'Unavailable' when there is no Bluetooth adapter
// present.
TEST_F(BluetoothSystemTest, State_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "Off" when the Bluetooth adapter is powered off.
TEST_F(BluetoothSystemTest, State_PoweredOffAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "On" when the Bluetooth adapter is powered on.
TEST_F(BluetoothSystemTest, State_PoweredOnAdapter) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state changes to On when the adapter turns on and then changes
// to Off when the adapter turns off.
TEST_F(BluetoothSystemTest, State_PoweredOnThenOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();

  auto system = CreateBluetoothSystem();

  // The adapter is initially powered off.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn adapter on.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  ResetResults();

  // Turn adapter off.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is updated as expected when removing and re-adding the
// same adapter.
TEST_F(BluetoothSystemTest, State_AdapterRemoved) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

    // The adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove the adapter. The state should change to Unavailable.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved();

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Add the adapter again; it's off by default.
  test_bluetooth_adapter_client_->SimulateAdapterAdded();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is updated as expected when replacing the adapter with a
// different adapter.
TEST_F(BluetoothSystemTest, State_AdapterReplaced) {
  // Start with a powered on adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(
      kDefaultAdapterObjectPathStr);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove the adapter. The state should change to Unavailable.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(
      kDefaultAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Add a different adapter. It's off by default.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(
      kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is correctly updated when adding and removing multiple
// adapters.
TEST_F(BluetoothSystemTest, State_AddAndRemoveMultipleAdapters) {
  // Start with a powered on default adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(
      kDefaultAdapterObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The default adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Add an alternate adapter. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(
      kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove the default adapter. We should retrieve the state from the
  // alternate adapter..
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(
      kDefaultAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
  ResetResults();

  // Change the alternate adapter's state to On.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      true, kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  ResetResults();

  // Add the default adapter again. We should still retrieve the state from
  // the alternate adapter.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(
      kDefaultAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that an extra adapter changing state does not interfer with the state.
TEST_F(BluetoothSystemTest, State_ChangeStateMultipleAdapters) {
  // Start with a powered on default adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(
      kDefaultAdapterObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The default adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Add an extra alternate adapter. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(
      kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn the alternate adapter on. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      true, kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn the alternate adapter off. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      false, kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that SetPowered fails if there is no adapter.
TEST_F(BluetoothSystemTest, SetPowered_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(
      mojom::BluetoothSystem::SetPoweredResult::kFailedBluetoothUnavailable,
      SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(
      mojom::BluetoothSystem::SetPoweredResult::kFailedBluetoothUnavailable,
      SetPoweredAndWait(system.get(), false));
}

// Tests setting powered to "Off" when the adapter is "Off" already.
TEST_F(BluetoothSystemTest, SetPoweredOff_SucceedsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  // The adapter is initially "Off" so a call to turn it "Off" should have no
  // effect but the call should still succeed.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
}

// Tests setting powered to "On" when the adapter is "On" already.
TEST_F(BluetoothSystemTest, SetPoweredOn_SucceedsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // The adapter is initially "On" so a call to turn it "On" should have no
  // effect but the call should still succeed.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), true));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
}

// Tests successfully setting powered to "Off when the adapter is "On".
TEST_F(BluetoothSystemTest, SetPoweredOff_SucceedsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));

  // Try to power off the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_FALSE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  // Simulate the adapter actually powering off.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests successfully setting powered to "On" when the adapter is "Off".
TEST_F(BluetoothSystemTest, SetPoweredOn_SucceedsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  // Try to power on the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), true));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_TRUE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  // Simulate the adapter actually powering on.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

// Tests failing to set powered to "Off when the adapter is "On".
TEST_F(BluetoothSystemTest, SetPoweredOff_FailsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(false);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_FALSE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning,
                         mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

// Tests failing to set powered to "On" when the adapter is "Off".
TEST_F(BluetoothSystemTest, SetPoweredOn_FailsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(false);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            SetPoweredAndWait(system.get(), true));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_TRUE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning,
                         mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is correctly updated if the adapter is removed
// when a call to set powered to "On" is pending.
TEST_F(BluetoothSystemTest, SetPoweredOn_AdapterRemovedWhilePending) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  absl::optional<mojom::BluetoothSystem::SetPoweredResult> result;

  // Start a SetPowered call and wait for the state to change to kTransitioning.
  base::RunLoop set_powered_run_loop;
  system->SetPowered(true, base::BindLambdaForTesting(
                               [&](mojom::BluetoothSystem::SetPoweredResult r) {
                                 result = r;
                                 set_powered_run_loop.Quit();
                               }));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  // Simulate the adapter being removed. This immediately changes the "powered"
  // property of the adapter to `false` and then removes the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved();
  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Wait for SetPowered() to reply.
  set_powered_run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            result.value());

  // There should not be any state changes when SetPowered eventually fails.
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is correctly updated if the adapter is removed
// when a call to set powered to "Off" is pending.
TEST_F(BluetoothSystemTest, SetPoweredOff_AdapterRemovedWhilePending) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  absl::optional<mojom::BluetoothSystem::SetPoweredResult> result;

  // Start a SetPowered call and wait for the state to change to kTransitioning.
  base::RunLoop set_powered_run_loop;
  system->SetPowered(false,
                     base::BindLambdaForTesting(
                         [&](mojom::BluetoothSystem::SetPoweredResult r) {
                           result = r;
                           set_powered_run_loop.Quit();
                         }));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  // Simulate the adapter being removed. This immediately changes the "powered"
  // property of the adapter to `false` and then removes the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved();
  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Wait for SetPowered() to reply.
  set_powered_run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            result.value());

  // There should not be any state changes when SetPowered eventually fails.
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests power off call with pending power off call.
TEST_F(BluetoothSystemTest, SetPoweredOff_PendingSetPoweredOff) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();
  ASSERT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));

  // Start powering off BT and wait for the state to change to
  // kTransitioning.
  base::RunLoop run_loop;
  absl::optional<mojom::BluetoothSystem::SetPoweredResult>
      set_powered_off_result;
  system->SetPowered(false,
                     base::BindLambdaForTesting(
                         [&](mojom::BluetoothSystem::SetPoweredResult r) {
                           set_powered_off_result = r;
                           run_loop.Quit();
                         }));

  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_FALSE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());

  ResetResults();
  test_bluetooth_adapter_client_->ResetCallCount();

  // Try to power off BT; should fail with kInProgress.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedInProgress,
            SetPoweredAndWait(system.get(), false));

  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector(), on_state_changed_states_);

  ResetResults();

  // Finish initial call to power off BT.
  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(true);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            set_powered_off_result.value());
}

// Tests power off call with pending power on call.
TEST_F(BluetoothSystemTest, SetPoweredOff_PendingSetPoweredOn) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();

  auto system = CreateBluetoothSystem();
  ASSERT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));

  // Start powering on BT and wait for the state to change to
  // kTransitioning.
  base::RunLoop run_loop;
  absl::optional<mojom::BluetoothSystem::SetPoweredResult>
      set_powered_on_result;
  system->SetPowered(true, base::BindLambdaForTesting(
                               [&](mojom::BluetoothSystem::SetPoweredResult r) {
                                 set_powered_on_result = r;
                                 run_loop.Quit();
                               }));

  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_TRUE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());

  ResetResults();
  test_bluetooth_adapter_client_->ResetCallCount();

  // Try to power off BT; should fail with kInProgress.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedInProgress,
            SetPoweredAndWait(system.get(), false));

  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector(), on_state_changed_states_);

  ResetResults();

  // Finish initial call to power on BT.
  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(true);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            set_powered_on_result.value());
}

// Tests power on call with pending power off call.
TEST_F(BluetoothSystemTest, SetPoweredOn_PendingSetPoweredOff) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();
  ASSERT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));

  // Start powering off BT and wait for the state to change to
  // kTransitioning.
  base::RunLoop run_loop;
  absl::optional<mojom::BluetoothSystem::SetPoweredResult>
      set_powered_off_result;
  system->SetPowered(false,
                     base::BindLambdaForTesting(
                         [&](mojom::BluetoothSystem::SetPoweredResult r) {
                           set_powered_off_result = r;
                           run_loop.Quit();
                         }));

  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_FALSE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());

  ResetResults();
  test_bluetooth_adapter_client_->ResetCallCount();

  // Try to power on BT; should fail with kInProgress.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedInProgress,
            SetPoweredAndWait(system.get(), true));

  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector(), on_state_changed_states_);

  ResetResults();

  // Finish initial call to power off BT.
  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(true);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            set_powered_off_result.value());
}

// Tests power on call with pending power on call.
TEST_F(BluetoothSystemTest, SetPoweredOn_PendingSetPoweredOn) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();

  auto system = CreateBluetoothSystem();
  ASSERT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));

  // Start powering on BT and wait for the state to change to
  // kTransitioning.
  base::RunLoop run_loop;
  absl::optional<mojom::BluetoothSystem::SetPoweredResult>
      set_powered_on_result;
  system->SetPowered(true, base::BindLambdaForTesting(
                               [&](mojom::BluetoothSystem::SetPoweredResult r) {
                                 set_powered_on_result = r;
                                 run_loop.Quit();
                               }));

  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_TRUE(test_bluetooth_adapter_client_->GetLastSetPoweredValue());

  ResetResults();
  test_bluetooth_adapter_client_->ResetCallCount();

  // Try to power on BT; should fail with kInProgress.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedInProgress,
            SetPoweredAndWait(system.get(), true));

  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector(), on_state_changed_states_);

  ResetResults();

  // Finish initial call to power on BT.
  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(true);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            set_powered_on_result.value());
}

// Tests scan state is kNotScanning when there is no adapter.
TEST_F(BluetoothSystemTest, ScanState_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state is kNotScanning when the adapter is not scanning.
TEST_F(BluetoothSystemTest, ScanState_NotScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  // Added adapters are not scanning by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state is kScanning when the adapter is scanning.
TEST_F(BluetoothSystemTest, ScanState_Scanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state changes to kScanning when the adapter starts scanning and
// then changes to kNotScanning when the adapter stops scanning.
TEST_F(BluetoothSystemTest, ScanState_ScanningThenNotScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Adapter starts scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Adapter stops scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
}

// Tests scan state is updated as expected when removing and re-adding the same
// adapter.
TEST_F(BluetoothSystemTest, ScanState_AdapterRemoved) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);

  auto system = CreateBluetoothSystem();

  // The adapter is initially scanning.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));

  // Remove the adapter. The state should change to not scanning.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Add the adapter again; it's not scanning by default.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // The adapter starts scanning again.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
}

// Tests that scan state is updated as expected when replacing the adapter with
// a different adapter.
TEST_F(BluetoothSystemTest, ScanState_AdapterReplaced) {
  // Start with a scanning adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(
      kDefaultAdapterObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      true, kDefaultAdapterObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The adapter is initially scanning.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));

  // Remove the adapter. The state should change to kNotScanning.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(
      kDefaultAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Add a different adapter. It's not scanning by default.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(
      kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // The new adapter starts scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      true, kAlternateAdapterObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
}

// Tests that StartScan fails if there is no adapter.
TEST_F(BluetoothSystemTest, StartScan_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(
      mojom::BluetoothSystem::StartScanResult::kFailedBluetoothUnavailable,
      StartScanAndWait(system.get()));
}

// Tests that StartScan fails if the adapter is "Off".
TEST_F(BluetoothSystemTest, StartScan_AdapterOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(
      mojom::BluetoothSystem::StartScanResult::kFailedBluetoothUnavailable,
      StartScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStartDiscoveryCallCount());
}

// Tests that StartScan succeeds and the scan state is correctly updated.
TEST_F(BluetoothSystemTest, StartScan_Succeeds) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));

  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::StartScanResult::kSuccess,
            StartScanAndWait(system.get()));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetStartDiscoveryCallCount());

  // TODO(ortuno): Test for kTransitioning once implemented.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector(), on_scan_state_changed_states_);
  ResetResults();

  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
}

// Tests that StartScan fails and the scan state is correctly updated.
TEST_F(BluetoothSystemTest, StartScan_Fails) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));

  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(false);
  EXPECT_EQ(mojom::BluetoothSystem::StartScanResult::kFailedUnknownReason,
            StartScanAndWait(system.get()));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetStartDiscoveryCallCount());

  // TODO(ortuno): Test for kTransitioning once implemented.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector(), on_scan_state_changed_states_);
}

// Tests that StartScan fails when the adapter is powering on.
TEST_F(BluetoothSystemTest, StartScan_FailsDuringPowerOn) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  // Start powering on the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), true));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  ResetResults();

  // Start scan should fail without sending the command to the adapter.
  EXPECT_EQ(
      mojom::BluetoothSystem::StartScanResult::kFailedBluetoothUnavailable,
      StartScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStartDiscoveryCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Finish powering on the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

// Tests that StartScan fails when the adapter is powering off.
TEST_F(BluetoothSystemTest, StartScan_FailsDuringPowerOff) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Start powering off the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  ResetResults();

  // Start scan should fail without sending the command to the adapter.
  EXPECT_EQ(
      mojom::BluetoothSystem::StartScanResult::kFailedBluetoothUnavailable,
      StartScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStartDiscoveryCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Finish powering off the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that StopScan fails if there is no adapter.
TEST_F(BluetoothSystemTest, StopScan_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kFailedBluetoothUnavailable,
            StopScanAndWait(system.get()));
}

// Tests that StopScan fails if the adapter is "Off".
TEST_F(BluetoothSystemTest, StopScan_AdapterOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kFailedBluetoothUnavailable,
            StopScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStopDiscoveryCallCount());
}

// Tests that StopScan succeeds and the scan state is correctly updated.
TEST_F(BluetoothSystemTest, StopScan_Succeeds) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Successfully start scanning.
  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  StartScanAndWait(system.get());
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  ResetResults();

  test_bluetooth_adapter_client_->SetNextStopDiscoveryResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kSuccess,
            StopScanAndWait(system.get()));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetStopDiscoveryCallCount());

  // TODO(ortuno): Test for kTransitioning once implemented.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector(), on_scan_state_changed_states_);
  ResetResults();

  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
}

// Tests that StopScan fails and the scan state is correctly updated.
TEST_F(BluetoothSystemTest, StopScan_Fails) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Successfully start scanning.
  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  StartScanAndWait(system.get());
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  ResetResults();

  test_bluetooth_adapter_client_->SetNextStopDiscoveryResponse(false);
  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kFailedUnknownReason,
            StopScanAndWait(system.get()));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetStopDiscoveryCallCount());

  // TODO(ortuno): Test for kTransitioning once implemented.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector(), on_scan_state_changed_states_);
}

// Tests that StopScan fails if when the adapter is powering on.
TEST_F(BluetoothSystemTest, StopScan_FailsDuringPowerOn) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded();
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  // Start powering on the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), true));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  ResetResults();

  // Stop scan should fail without sending the command to the adapter.
  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kFailedBluetoothUnavailable,
            StopScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStopDiscoveryCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Finish powering on the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

TEST_F(BluetoothSystemTest, StopScan_FailsDuringPowerOff) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Start scanning.
  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::StartScanResult::kSuccess,
            StartScanAndWait(system.get()));
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));

  // Start powering off the adapter.
  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system.get(), false));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system.get()));
  ResetResults();

  // Stop scan should fail without sending the command to the adapter.
  EXPECT_EQ(mojom::BluetoothSystem::StopScanResult::kFailedBluetoothUnavailable,
            StopScanAndWait(system.get()));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetStopDiscoveryCallCount());
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Finish powering off the adapter.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system.get()));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the scan state is correctly updated if the adapter is removed
// during scanning.
TEST_F(BluetoothSystemTest, Scan_AdapterRemovedWhileScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Start scanning.
  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  StartScanAndWait(system.get());
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);
  ASSERT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  ResetResults();

  // Remove the adapter. Scan state should change to kNotScanning.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
}

// Tests that the scan state is correctly updated if the adapter turns off
// during scanning.
TEST_F(BluetoothSystemTest, Scan_PowerOffWhileScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();

  auto system = CreateBluetoothSystem();

  // Start scanning.
  test_bluetooth_adapter_client_->SetNextStartDiscoveryResponse(true);
  StartScanAndWait(system.get());
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(true);
  ASSERT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system.get()));
  ResetResults();

  // Power off the adapter. Scan state should change to kNotScanning.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(false);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system.get()));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
}

// Tests addresses are parsed correctly.
TEST_F(BluetoothSystemTest, GetAvailableDevices_AddressParser) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  auto system = CreateBluetoothSystem();

  static const struct {
    std::string object_path;
    std::string address;
  } device_cases[] = {
      // Invalid addresses
      {"1", "00:11"},                 // Too short
      {"2", "00:11:22:AA:BB:CC:DD"},  // Too long
      {"3", "00|11|22|AA|BB|CC"},     // Invalid separator
      {"4", "00:11:22:XX:BB:CC"},     // Invalid character
      // Valid addresses
      {"5", "00:11:22:aa:bb:cc"},  // Lowercase
      {"6", "00:11:22:AA:BB:CC"},  // Uppercase
  };

  for (const auto& device : device_cases) {
    FakeDeviceOptions fake_options(device.object_path);
    fake_options.address = device.address;
    test_bluetooth_device_client_->SimulateDeviceAdded(fake_options);
  }

  auto devices = GetAvailableDevicesAndWait(system.get());
  ASSERT_EQ(2u, devices.size());

  const std::array<uint8_t, 6> expected_address = {0x00, 0x11, 0x22,
                                                   0xAA, 0xBB, 0xCC};
  EXPECT_EQ(expected_address, devices[0]->address);
  EXPECT_EQ(expected_address, devices[1]->address);
}

// Tests all properties of devices are returned correctly.
TEST_F(BluetoothSystemTest, GetAvailableDevices) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  auto system = CreateBluetoothSystem();

  {
    FakeDeviceOptions fake_options("1");
    fake_options.address = kDefaultDeviceAddressStr;
    fake_options.name = "Fake Device";
    fake_options.paired = true;
    fake_options.connected = true;
    test_bluetooth_device_client_->SimulateDeviceAdded(fake_options);
    test_bluetooth_admin_policy_client_->SimulateAdminPolicyAdded(
        fake_options.object_path, /*is_blocked_by_policy=*/true);
  }
  {
    FakeDeviceOptions fake_options("2");
    fake_options.address = kAlternateDeviceAddressStr;
    fake_options.name = absl::nullopt;
    fake_options.paired = false;
    fake_options.connected = false;
    test_bluetooth_device_client_->SimulateDeviceAdded(fake_options);
    test_bluetooth_admin_policy_client_->SimulateAdminPolicyAdded(
        fake_options.object_path, /*is_blocked_by_policy=*/false);
  }

  auto devices = GetAvailableDevicesAndWait(system.get());
  ASSERT_EQ(2u, devices.size());

  mojom::BluetoothDeviceInfoPtr device_with_name;
  mojom::BluetoothDeviceInfoPtr device_without_name;
  if (devices[0]->address == kDefaultDeviceAddressArray) {
    device_with_name = std::move(devices[0]);
    device_without_name = std::move(devices[1]);
  } else {
    device_with_name = std::move(devices[1]);
    device_without_name = std::move(devices[0]);
  }

  EXPECT_EQ(device_with_name->name.value(), "Fake Device");
  EXPECT_TRUE(device_with_name->is_paired);
  EXPECT_TRUE(device_with_name->is_blocked_by_policy);
  EXPECT_EQ(device_with_name->connection_state,
            mojom::BluetoothDeviceInfo::ConnectionState::kConnected);

  EXPECT_FALSE(!!device_without_name->name);
  EXPECT_FALSE(device_without_name->is_paired);
  EXPECT_FALSE(device_without_name->is_blocked_by_policy);
  EXPECT_EQ(device_without_name->connection_state,
            mojom::BluetoothDeviceInfo::ConnectionState::kNotConnected);
}

// Tests that if a device existed before BluetoothSystem was created, the
// device is still returned when calling GetAvailableDevices().
TEST_F(BluetoothSystemTest, GetAvailableDevices_ExistingDevice) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter();
  test_bluetooth_device_client_->SimulateDeviceAdded(FakeDeviceOptions());

  auto system = CreateBluetoothSystem();

  auto devices = GetAvailableDevicesAndWait(system.get());
  ASSERT_EQ(1u, devices.size());

  EXPECT_EQ(kDefaultDeviceAddressArray, devices[0]->address);
  EXPECT_FALSE(devices[0]->name);
  EXPECT_EQ(mojom::BluetoothDeviceInfo::ConnectionState::kNotConnected,
            devices[0]->connection_state);
  EXPECT_FALSE(devices[0]->is_paired);
}

}  // namespace device
