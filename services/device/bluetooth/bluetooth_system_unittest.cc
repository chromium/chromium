// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

constexpr const char kFooObjectPathStr[] = "fake/hci0";
constexpr const char kBarObjectPathStr[] = "fake/hci1";

namespace {

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

    void ResetCallCount() { set_powered_call_count_ = 0; }

    void SetNextSetPoweredResponse(bool next_response) {
      DCHECK(!next_set_powered_response_);
      next_set_powered_response_ = next_response;
    }

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
        base::Optional<bool> response;
        response.swap(next_set_powered_response_);
        callback.Run(response.value());
        return;
      }
      NOTIMPLEMENTED();
    }

    size_t set_powered_call_count_ = 0;
    base::Optional<bool> next_set_powered_response_;
  };

  TestBluetoothAdapterClient() = default;
  ~TestBluetoothAdapterClient() override = default;

  void ResetCallCount() {
    for (auto& path_to_properties : adapter_object_paths_to_properties_) {
      path_to_properties.second.ResetCallCount();
    }
  }

  // Low level methods to simulate events and operations.

  // Simulates a new adapter with |object_path_str|. Its properties are empty,
  // 0, or false.
  void SimulateAdapterAdded(const std::string& object_path_str) {
    dbus::ObjectPath object_path(object_path_str);

    ObjectPathToProperties::iterator it;
    bool was_inserted;
    std::tie(it, was_inserted) = adapter_object_paths_to_properties_.emplace(
        object_path,
        base::BindRepeating(&TestBluetoothAdapterClient::OnPropertyChanged,
                            base::Unretained(this), object_path));
    DCHECK(was_inserted);

    for (auto& observer : observers_)
      observer.AdapterAdded(object_path);
  }

  // Simulates the adapter at |object_path_str| being removed.
  void SimulateAdapterRemoved(const std::string& object_path_str) {
    dbus::ObjectPath object_path(object_path_str);

    // Properties are set to empty, 0, or false right before AdapterRemoved is
    // called.
    GetProperties(object_path)->powered.ReplaceValue(false);
    GetProperties(object_path)->discovering.ReplaceValue(false);

    // When BlueZ calls into AdapterRemoved, the adapter is still exposed
    // through GetAdapters() and its properties are still accessible.
    for (auto& observer : observers_)
      observer.AdapterRemoved(object_path);

    size_t removed = adapter_object_paths_to_properties_.erase(object_path);
    DCHECK_EQ(1u, removed);
  }

  // Simulates adapter at |object_path_str| changing its powered state to
  // |powered|.
  void SimulateAdapterPowerStateChanged(const std::string& object_path_str,
                                        bool powered) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->powered.ReplaceValue(powered);
  }

  void SetNextSetPoweredResponse(const std::string& object_path_str,
                                 bool response) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->SetNextSetPoweredResponse(response);
  }

  size_t GetSetPoweredCallCount(const std::string& object_path_str) {
    auto it = adapter_object_paths_to_properties_.find(
        dbus::ObjectPath(object_path_str));
    DCHECK(it != adapter_object_paths_to_properties_.end());

    return it->second.set_powered_call_count_;
  }

  // Simulates the adapter changing states in response to a previous call to
  // change the "powered" property.
  void SimulateSetPoweredCompleted(const std::string& object_path_str) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->powered.ReplaceValueWithSetValue();
  }

  // Simulates adapter at |object_path_str| changing its discovering state to
  // |powered|.
  void SimulateAdapterDiscoveringStateChanged(
      const std::string& object_path_str,
      bool discovering) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->discovering.ReplaceValue(discovering);
  }

  // Helper methods to perform multiple common operations.

  // Simultes adding an adapter and it changing its state to powered On.
  void SimulatePoweredOnAdapter(const std::string& object_path_str) {
    SimulateAdapterAdded(object_path_str);
    SimulateAdapterPowerStateChanged(object_path_str, true);
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
    return &(it->second);
  }

  void StartDiscovery(const dbus::ObjectPath& object_path,
                      const base::Closure& callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void StopDiscovery(const dbus::ObjectPath& object_path,
                     const base::Closure& callback,
                     ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void PauseDiscovery(const dbus::ObjectPath& object_path,
                      const base::Closure& callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void UnpauseDiscovery(const dbus::ObjectPath& object_path,
                        const base::Closure& callback,
                        ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void RemoveDevice(const dbus::ObjectPath& object_path,
                    const dbus::ObjectPath& device_path,
                    const base::Closure& callback,
                    ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                          const DiscoveryFilter& discovery_filter,
                          const base::Closure& callback,
                          ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void CreateServiceRecord(const dbus::ObjectPath& object_path,
                           const bluez::BluetoothServiceRecordBlueZ& record,
                           const ServiceRecordCallback& callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                           uint32_t handle,
                           const base::Closure& callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_) {
      observer.AdapterPropertyChanged(object_path, property_name);
    }
  }

  using ObjectPathToProperties = std::map<dbus::ObjectPath, Properties>;
  ObjectPathToProperties adapter_object_paths_to_properties_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace

class BluetoothSystemTest : public DeviceServiceTestBase,
                            public mojom::BluetoothSystemClient {
 public:
  BluetoothSystemTest() = default;
  ~BluetoothSystemTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    connector()->BindInterface(mojom::kServiceName, &system_factory_);

    auto test_bluetooth_adapter_client =
        std::make_unique<TestBluetoothAdapterClient>();
    test_bluetooth_adapter_client_ = test_bluetooth_adapter_client.get();

    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetAlternateBluetoothAdapterClient(
        std::move(test_bluetooth_adapter_client));
  }

  // Helper methods to avoid AsyncWaiter boilerplate.
  mojom::BluetoothSystem::State GetStateAndWait(
      const mojom::BluetoothSystemPtr& system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system.get());

    mojom::BluetoothSystem::State state;
    async_waiter.GetState(&state);

    return state;
  }

  mojom::BluetoothSystem::SetPoweredResult SetPoweredAndWait(
      const mojom::BluetoothSystemPtr& system,
      bool powered) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system.get());

    mojom::BluetoothSystem::SetPoweredResult result;
    async_waiter.SetPowered(powered, &result);

    return result;
  }

  mojom::BluetoothSystem::ScanState GetScanStateAndWait(
      const mojom::BluetoothSystemPtr& system) {
    mojom::BluetoothSystemAsyncWaiter async_waiter(system.get());

    mojom::BluetoothSystem::ScanState scan_state;
    async_waiter.GetScanState(&scan_state);

    return scan_state;
  }

  // mojom::BluetoothSystemClient
  void OnStateChanged(mojom::BluetoothSystem::State state) override {
    on_state_changed_states_.push_back(state);
  }

  void OnScanStateChanged(mojom::BluetoothSystem::ScanState state) override {
    on_scan_state_changed_states_.push_back(state);
  }

 protected:
  mojom::BluetoothSystemPtr CreateBluetoothSystem() {
    mojom::BluetoothSystemClientPtr client_ptr;
    system_client_binding_.Bind(mojo::MakeRequest(&client_ptr));

    mojom::BluetoothSystemPtr system_ptr;
    system_factory_->Create(mojo::MakeRequest(&system_ptr),
                            std::move(client_ptr));
    return system_ptr;
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

  mojom::BluetoothSystemFactoryPtr system_factory_;

  TestBluetoothAdapterClient* test_bluetooth_adapter_client_;

  mojo::Binding<mojom::BluetoothSystemClient> system_client_binding_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSystemTest);
};

// Tests that the Create method for BluetoothSystemFactory works.
TEST_F(BluetoothSystemTest, FactoryCreate) {
  mojom::BluetoothSystemPtr system_ptr;
  mojo::Binding<mojom::BluetoothSystemClient> client_binding(this);

  mojom::BluetoothSystemClientPtr client_ptr;
  client_binding.Bind(mojo::MakeRequest(&client_ptr));

  EXPECT_FALSE(system_ptr.is_bound());

  system_factory_->Create(mojo::MakeRequest(&system_ptr),
                          std::move(client_ptr));
  base::RunLoop run_loop;
  system_ptr.FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(system_ptr.is_bound());
}

// Tests that the state is 'Unavailable' when there is no Bluetooth adapter
// present.
TEST_F(BluetoothSystemTest, State_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "Off" when the Bluetooth adapter is powered off.
TEST_F(BluetoothSystemTest, State_PoweredOffAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "On" when the Bluetooth adapter is powered on.
TEST_F(BluetoothSystemTest, State_PoweredOnAdapter) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state changes to On when the adapter turns on and then changes
// to Off when the adapter turns off.
TEST_F(BluetoothSystemTest, State_PoweredOnThenOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The adapter is initially powered off.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn adapter on.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  ResetResults();

  // Turn adapter off.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is updated as expected when removing and re-adding the
// same adapter.
TEST_F(BluetoothSystemTest, State_AdapterRemoved) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

    // The adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove the adapter. The state should change to Unavailable.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Add the adapter again; it's off by default.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is updated as expected when replacing the adapter with a
// different adapter.
TEST_F(BluetoothSystemTest, State_AdapterReplaced) {
  // Start with a powered on adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove the adapter. The state should change to Unavailable.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                         mojom::BluetoothSystem::State::kUnavailable}),
            on_state_changed_states_);
  ResetResults();

  // Add a different adapter. It's off by default.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests that the state is correctly updated when adding and removing multiple
// adapters.
TEST_F(BluetoothSystemTest, State_AddAndRemoveMultipleAdapters) {
  // Start with a powered on "foo" adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The "foo" adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Add an extra "bar" adapter. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Remove "foo". We should retrieve the state from "bar".
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
  ResetResults();

  // Change "bar"'s state to On.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kBarObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
  ResetResults();

  // Add "foo" again. We should still retrieve the state from "bar".
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that an extra adapter changing state does not interfer with the state.
TEST_F(BluetoothSystemTest, State_ChangeStateMultipleAdapters) {
  // Start with a powered on "foo" adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The "foo" adapter is initially powered on.
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Add an extra "bar" adapter. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn "bar" on. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kBarObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());

  // Turn "bar" off. The state should not change.
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kBarObjectPathStr, false);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that SetPowered fails if there is no adapter.
TEST_F(BluetoothSystemTest, SetPowered_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kBluetoothUnavailable,
            SetPoweredAndWait(system, false));
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kBluetoothUnavailable,
            SetPoweredAndWait(system, false));
}

// Tests setting powered to "Off" when the adapter is "Off" already.
TEST_F(BluetoothSystemTest, SetPoweredOff_SucceedsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  // The adapter is initially "Off" so a call to turn it "Off" should have no
  // effect but the call should still succeed.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system, false));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
}

// Tests setting powered to "On" when the adapter is "On" already.
TEST_F(BluetoothSystemTest, SetPoweredOn_SucceedsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  // The adapter is initially "On" so a call to turn it "On" should have no
  // effect but the call should still succeed.
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system, true));
  EXPECT_EQ(0u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
}

// Tests successfully setting powered to "Off when the adapter is "On".
TEST_F(BluetoothSystemTest, SetPoweredOff_SucceedsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(kFooObjectPathStr,
                                                            true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system, false));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(
      kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests successfully setting powered to "On" when the adapter is "Off".
TEST_F(BluetoothSystemTest, SetPoweredOn_SucceedsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(kFooObjectPathStr,
                                                            true);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kSuccess,
            SetPoweredAndWait(system, true));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
  EXPECT_EQ(mojom::BluetoothSystem::State::kTransitioning,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning}),
            on_state_changed_states_);
  ResetResults();

  test_bluetooth_adapter_client_->SimulateSetPoweredCompleted(
      kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

// Tests failing to set powered to "Off when the adapter is "On".
TEST_F(BluetoothSystemTest, SetPoweredOff_FailsAdapterInitiallyOn) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(kFooObjectPathStr,
                                                            false);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            SetPoweredAndWait(system, false));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning,
                         mojom::BluetoothSystem::State::kPoweredOn}),
            on_state_changed_states_);
}

// Tests failing to set powered to "On" when the adapter is "Off".
TEST_F(BluetoothSystemTest, SetPoweredOn_FailsAdapterInitiallyOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  test_bluetooth_adapter_client_->SetNextSetPoweredResponse(kFooObjectPathStr,
                                                            false);
  EXPECT_EQ(mojom::BluetoothSystem::SetPoweredResult::kFailedUnknownReason,
            SetPoweredAndWait(system, true));
  EXPECT_EQ(1u, test_bluetooth_adapter_client_->GetSetPoweredCallCount(
                    kFooObjectPathStr));
  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            GetStateAndWait(system));
  EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kTransitioning,
                         mojom::BluetoothSystem::State::kPoweredOff}),
            on_state_changed_states_);
}

// Tests scan state is kNotScanning when there is no adapter.
TEST_F(BluetoothSystemTest, ScanState_NoAdapter) {
  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state is kNotScanning when the adapter is not scanning.
TEST_F(BluetoothSystemTest, ScanState_NotScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);
  // Added adapters are not scanning by default.

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state is kScanning when the adapter is scanning.
TEST_F(BluetoothSystemTest, ScanState_Scanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());
}

// Tests scan state changes to kScanning when the adapter starts scanning and
// then changes to kNotScanning when the adapter stops scanning.
TEST_F(BluetoothSystemTest, ScanState_ScanningThenNotScanning) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // Adapter starts scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Adapter stops scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, false);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
}

// Tests scan state is updated as expected when removing and re-adding the same
// adapter.
TEST_F(BluetoothSystemTest, ScanState_AdapterRemoved) {
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  // The adapter is initially scanning.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));

  // Remove the adapter. The state should change to not scanning.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Add the adapter again; it's not scanning by default.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // The adapter starts scanning again.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
}

// Tests that scan state is updated as expected when replacing the adapter with
// a different adapter.
TEST_F(BluetoothSystemTest, ScanState_AdapterReplaced) {
  // Start with a scanning adapter.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  // The adapter is initially scanning.
  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));

  // Remove the adapter. The state should change to kNotScanning.
  test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kNotScanning}),
            on_scan_state_changed_states_);
  ResetResults();

  // Add a different adapter. It's not scanning by default.
  test_bluetooth_adapter_client_->SimulatePoweredOnAdapter(kBarObjectPathStr);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kNotScanning,
            GetScanStateAndWait(system));
  EXPECT_TRUE(on_scan_state_changed_states_.empty());

  // The new adapter starts scanning.
  test_bluetooth_adapter_client_->SimulateAdapterDiscoveringStateChanged(
      kBarObjectPathStr, true);

  EXPECT_EQ(mojom::BluetoothSystem::ScanState::kScanning,
            GetScanStateAndWait(system));
  EXPECT_EQ(ScanStateVector({mojom::BluetoothSystem::ScanState::kScanning}),
            on_scan_state_changed_states_);
}

}  // namespace device
