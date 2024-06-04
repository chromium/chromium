// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "device/bluetooth/test/test_bluetooth_advertisement_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif BUILDFLAG(IS_APPLE)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using device::BluetoothDevice;

namespace device {

void AddDeviceFilterWithUUID(BluetoothDiscoveryFilter* filter,
                             BluetoothUUID uuid) {
  BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(uuid);
  filter->AddDeviceFilter(device_filter);
}

namespace {

class TestBluetoothAdapter final : public BluetoothAdapter {
 public:
  TestBluetoothAdapter() = default;

  void Initialize(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  std::string GetAddress() const override { return ""; }

  std::string GetName() const override { return ""; }

  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override {}

  bool IsInitialized() const override { return false; }

  bool IsPresent() const override { return false; }

  bool IsPowered() const override { return false; }

  void SetPowered(bool powered,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {}

  bool IsDiscoverable() const override { return false; }

  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override {}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::TimeDelta GetDiscoverableTimeout() const override {
    return base::Microseconds(0);
  }
#endif

  bool IsDiscovering() const override { return false; }

  UUIDList GetUUIDs() const override { return UUIDList(); }

  void CreateRfcommService(const BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override {
  }

  void CreateL2capService(const BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override {}

  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override {}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override {}
  void ResetAdvertising(base::OnceClosure callback,
                        AdvertisementErrorCallback error_callback) override {}
  void ConnectDevice(
      const std::string& address,
      const std::optional<BluetoothDevice::AddressType>& address_type,
      ConnectDeviceCallback callback,
      ConnectDeviceErrorCallback error_callback) override {}
#endif

  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetServiceAllowList(const UUIDList& uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {}

  LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override {
    return LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported;
  }

  std::unique_ptr<BluetoothLowEnergyScanSession> StartLowEnergyScanSession(
      std::unique_ptr<BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<BluetoothLowEnergyScanSession::Delegate> delegate)
      override {
    return nullptr;
  }

  std::vector<BluetoothRole> GetSupportedRoles() override {
    return std::vector<BluetoothRole>{};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetStandardChromeOSAdapterName() override {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void OnStartDiscoverySessionQuitLoop(
      base::OnceClosure run_loop_quit,
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    std::move(run_loop_quit).Run();
    discovery_sessions_holder_.push(std::move(discovery_session));
  }

  void OnRemoveDiscoverySession(base::OnceClosure run_loop_quit) {
    ++callback_count_;
    std::move(run_loop_quit).Run();
  }

  void OnRemoveDiscoverySessionError(base::OnceClosure run_loop_quit) {
    ++callback_count_;
    std::move(run_loop_quit).Run();
  }

  void set_discovery_session_outcome(
      UMABluetoothDiscoverySessionOutcome outcome) {
    discovery_session_outcome_ = outcome;
  }

  void StopDiscoverySession(base::OnceClosure run_loop_quit) {
    auto split_run_loop = base::SplitOnceCallback(std::move(run_loop_quit));
    discovery_sessions_holder_.front()->Stop(
        base::BindOnce(&TestBluetoothAdapter::OnRemoveDiscoverySession, this,
                       std::move(split_run_loop.first)),
        base::BindOnce(&TestBluetoothAdapter::OnRemoveDiscoverySessionError,
                       this, std::move(split_run_loop.second)));
    discovery_sessions_holder_.pop();
  }

  void StopAllDiscoverySessions(base::OnceClosure run_loop_quit) {
    base::RepeatingClosure closure = base::BarrierClosure(
        discovery_sessions_holder_.size(), std::move(run_loop_quit));
    while (!discovery_sessions_holder_.empty()) {
      discovery_sessions_holder_.front()->Stop(
          base::BindLambdaForTesting([closure, this]() {
            ++callback_count_;
            closure.Run();
          }),
          base::BindOnce(&TestBluetoothAdapter::OnRemoveDiscoverySessionError,
                         this, closure));
      discovery_sessions_holder_.pop();
    }
  }

  void CleanupSessions() {
    // clear discovery_sessions_holder_
    base::queue<std::unique_ptr<BluetoothDiscoverySession>> empty_queue;
    std::swap(discovery_sessions_holder_, empty_queue);
  }

  void QueueStartRequests(base::OnceClosure run_loop_quit, int num_requests) {
    base::RepeatingClosure closure =
        base::BarrierClosure(num_requests, std::move(run_loop_quit));
    for (int i = 0; i < num_requests; ++i) {
      StartDiscoverySession(
          /*client_name=*/std::string(),
          base::BindLambdaForTesting(
              [closure, this](std::unique_ptr<device::BluetoothDiscoverySession>
                                  discovery_session) {
                ++callback_count_;
                discovery_sessions_holder_.push(std::move(discovery_session));
                closure.Run();
              }),
          closure);
    }
  }

  void StartSessionWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      base::OnceClosure run_loop_quit) {
    StartDiscoverySessionWithFilter(
        std::move(discovery_filter),
        /*client_name=*/std::string(),
        base::BindOnce(&TestBluetoothAdapter::OnStartDiscoverySessionQuitLoop,
                       this, std::move(run_loop_quit)),
        base::DoNothing());
  }

  void RunStartSessionWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> filter) {
    base::RunLoop loop;
    StartSessionWithFilter(std::move(filter), loop.QuitClosure());
    loop.Run();
  }

  // |discovery_sessions_holder_| is used to hold unique pointers of Discovery
  // Sessions so that the destructors don't get called and so we can test
  // removing them
  base::queue<std::unique_ptr<BluetoothDiscoverySession>>
      discovery_sessions_holder_;
  std::unique_ptr<BluetoothDiscoveryFilter> current_filter =
      std::make_unique<BluetoothDiscoveryFilter>();
  int callback_count_ = 0;
  bool is_discovering_ = false;

 protected:
  ~TestBluetoothAdapter() override = default;

  bool SetPoweredImpl(bool powered) override { return false; }

  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestBluetoothAdapter::SetFilter,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(discovery_filter), std::move(callback)));
  }

  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestBluetoothAdapter::SetFilter,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(discovery_filter), std::move(callback)));
  }

  void SetFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                 DiscoverySessionResultCallback callback) {
    bool is_error = discovery_session_outcome_ !=
                    UMABluetoothDiscoverySessionOutcome::SUCCESS;
    if (!is_error) {
      is_discovering_ = true;
      current_filter->CopyFrom(*discovery_filter.get());
    }
    std::move(callback).Run(is_error, discovery_session_outcome_);
  }

  void StopScan(DiscoverySessionResultCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestBluetoothAdapter::FakeOSStopScan,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void FakeOSStopScan(DiscoverySessionResultCallback callback) {
    is_discovering_ = false;
    std::move(callback).Run(/*is_error=*/false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override {}

 private:
  void PostDelayedTask(base::OnceClosure callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  UMABluetoothDiscoverySessionOutcome discovery_session_outcome_ =
      UMABluetoothDiscoverySessionOutcome::SUCCESS;

  // This must be the last field in the class so that weak pointers are
  // invalidated first.
  base::WeakPtrFactory<TestBluetoothAdapter> weak_ptr_factory_{this};
};

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  void RequestPinCode(BluetoothDevice* device) override {}
  void RequestPasskey(BluetoothDevice* device) override {}
  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override {}
  void DisplayPasskey(BluetoothDevice* device, uint32_t passkey) override {}
  void KeysEntered(BluetoothDevice* device, uint32_t entered) override {}
  void ConfirmPasskey(BluetoothDevice* device, uint32_t passkey) override {}
  void AuthorizePairing(BluetoothDevice* device) override {}
};

bool ServiceSetsEqual(
    std::vector<BluetoothLocalGattService*> services,
    std::initializer_list<BluetoothLocalGattService*> services_to_check) {
  using ServiceSet = std::set<BluetoothLocalGattService*,
                              bool (*)(BluetoothLocalGattService*,
                                       BluetoothLocalGattService*)>;
  auto comparator = [](BluetoothLocalGattService* a,
                       BluetoothLocalGattService* b) -> bool {
    return a->GetIdentifier() < b->GetIdentifier();
  };

  return ServiceSet(services.begin(), services.end(), comparator) ==
         ServiceSet(services_to_check, comparator);
}

}  // namespace

class BluetoothAdapterTest : public testing::Test {
 public:
  void SetUp() override { adapter_ = new TestBluetoothAdapter(); }

 protected:
  scoped_refptr<TestBluetoothAdapter> adapter_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothAdapterTest, NoDefaultPairingDelegate) {
  // Verify that when there is no registered pairing delegate, NULL is returned.
  EXPECT_TRUE(adapter_->DefaultPairingDelegate() == nullptr);
}

TEST_F(BluetoothAdapterTest, OneDefaultPairingDelegate) {
  // Verify that when there is one registered pairing delegate, it is returned.
  TestPairingDelegate delegate;

  adapter_->AddPairingDelegate(&delegate,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate, adapter_->DefaultPairingDelegate());
}

TEST_F(BluetoothAdapterTest, SamePriorityDelegates) {
  // Verify that when there are two registered pairing delegates of the same
  // priority, the first one registered is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter_->AddPairingDelegate(&delegate1,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter_->AddPairingDelegate(&delegate2,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate1, adapter_->DefaultPairingDelegate());

  // After unregistering the first, the second can be returned.
  adapter_->RemovePairingDelegate(&delegate1);

  EXPECT_EQ(&delegate2, adapter_->DefaultPairingDelegate());
}

TEST_F(BluetoothAdapterTest, HighestPriorityDelegate) {
  // Verify that when there are two registered pairing delegates, the one with
  // the highest priority is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter_->AddPairingDelegate(&delegate1,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter_->AddPairingDelegate(
      &delegate2, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  EXPECT_EQ(&delegate2, adapter_->DefaultPairingDelegate());
}

TEST_F(BluetoothAdapterTest, UnregisterDelegate) {
  // Verify that after unregistering a delegate, NULL is returned.
  TestPairingDelegate delegate;

  adapter_->AddPairingDelegate(&delegate,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter_->RemovePairingDelegate(&delegate);

  EXPECT_TRUE(adapter_->DefaultPairingDelegate() == nullptr);
}

TEST_F(BluetoothAdapterTest, GetMergedDiscoveryFilterEmpty) {
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;

  discovery_filter = adapter_->GetMergedDiscoveryFilter();
  EXPECT_TRUE(discovery_filter->IsDefault());
}

TEST_F(BluetoothAdapterTest, GetMergedDiscoveryFilterRegular) {
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;

  // having one reglar session should result in no filter
  std::unique_ptr<BluetoothDiscoveryFilter> resulting_filter =
      adapter_->GetMergedDiscoveryFilter();
  EXPECT_TRUE(resulting_filter->IsDefault());

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, TestQueueingLogic) {
  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(discovery_filter.get(), BluetoothUUID("1001"));

  auto discovery_filter2 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(discovery_filter2.get(), BluetoothUUID("1002"));

  // Start a discovery session
  base::RunLoop run_loop1;
  adapter_->StartSessionWithFilter(std::move(discovery_filter),
                                   run_loop1.QuitClosure());

  // Since the first request is still running we have not hit a single callback
  EXPECT_EQ(0, adapter_->callback_count_);

  // While our first Discovery Session is starting queue up another one with a
  // different filter.
  base::RunLoop run_loop2;
  adapter_->StartSessionWithFilter(std::move(discovery_filter2),
                                   run_loop2.QuitClosure());

  // Finish the first request.  This should automatically start the queued
  // request when completed.
  run_loop1.Run();
  EXPECT_EQ(1, adapter_->callback_count_);
  EXPECT_TRUE(adapter_->is_discovering_);
  adapter_->callback_count_ = 0;

  // While our second discovery session is starting queue up 4 start requests
  // with default filters.
  base::RunLoop run_loop3;
  adapter_->QueueStartRequests(run_loop3.QuitClosure(), 4);

  // Finish the second request.  This should start the 4 queued requests.
  run_loop2.Run();
  EXPECT_EQ(1, adapter_->callback_count_);
  adapter_->callback_count_ = 0;

  // Queue up stop requests for the 2 started sessions.
  base::RunLoop run_loop4;
  adapter_->StopAllDiscoverySessions(base::DoNothing());
  // Confirm that no callbacks were hit and the filter has not changed.
  EXPECT_EQ(0, adapter_->callback_count_);
  EXPECT_FALSE(adapter_->current_filter->IsDefault());

  // Run the 4 queued default requests. This should call all 4 callbacks from
  // the start requests and automatically call the start the queue stop
  // requests.  These will short circuit and return immediately because the
  // filter remains the default filter.
  run_loop3.Run();
  EXPECT_EQ(6, adapter_->callback_count_);
  EXPECT_TRUE(adapter_->current_filter->IsDefault());

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, ShortCircuitUpdateTest) {
  auto discovery_filter_default1 = std::make_unique<BluetoothDiscoveryFilter>();
  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-30);

  base::RunLoop run_loop;
  adapter_->StartSessionWithFilter(std::move(discovery_filter_default1),
                                   run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(1, adapter_->callback_count_);

  // This time since there is no actual change being made the adapter should
  // short circuit the callback immediately with success
  auto discovery_filter_default2 = std::make_unique<BluetoothDiscoveryFilter>();
  adapter_->StartSessionWithFilter(std::move(discovery_filter_default2),
                                   base::DoNothing());
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(2, adapter_->callback_count_);

  // This filter is different but is already covered under the default filter so
  // it should still short circuit
  adapter_->StartSessionWithFilter(std::move(discovery_filter),
                                   base::DoNothing());
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(3, adapter_->callback_count_);

  // End the first discovery session which still short circuits because there is
  // still a default session active
  adapter_->StopDiscoverySession(base::DoNothing());
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(4, adapter_->callback_count_);

  // When removing the second default filter we should now actually update
  // because discovery_filter is the only filter left
  base::RunLoop run_loop2;
  adapter_->StopDiscoverySession(run_loop2.QuitClosure());
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(4, adapter_->callback_count_);
  run_loop2.Run();
  EXPECT_FALSE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(5, adapter_->callback_count_);

  //  Adding another default but not finishing the update
  base::RunLoop run_loop3;
  auto discovery_filter_default3 = std::make_unique<BluetoothDiscoveryFilter>();
  adapter_->StartSessionWithFilter(std::move(discovery_filter_default3),
                                   run_loop3.QuitClosure());
  EXPECT_FALSE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(5, adapter_->callback_count_);

  // Queue up another request that should short circuit and return success when
  // default filter 3 updates
  auto discovery_filter_default4 = std::make_unique<BluetoothDiscoveryFilter>();
  adapter_->StartSessionWithFilter(std::move(discovery_filter_default4),
                                   base::DoNothing());
  EXPECT_FALSE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(5, adapter_->callback_count_);

  // Running the loop default filter 3 will update the the OS filter then then
  // start processing the queued request.  The queued request does not change
  // the filter so it should automatically short circuit and return success.
  run_loop3.Run();
  EXPECT_TRUE(adapter_->current_filter->IsDefault());
  EXPECT_EQ(7, adapter_->callback_count_);

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, GetMergedDiscoveryFilterRssi) {
  int16_t resulting_rssi;
  uint16_t resulting_pathloss;
  std::unique_ptr<BluetoothDiscoveryFilter> resulting_filter;

  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-30);

  auto discovery_filter2 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter2->SetRSSI(-65);

  // Make sure adapter has one session without filtering.
  adapter_->RunStartSessionWithFilter(std::move(discovery_filter));

  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-30, resulting_rssi);

  adapter_->RunStartSessionWithFilter(std::move(discovery_filter2));

  // result of merging two rssi values should be lower one
  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-65, resulting_rssi);

  BluetoothDiscoveryFilter* df3 =
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE);
  df3->SetPathloss(60);
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  // when rssi and pathloss are merged, both should be cleared, because there is
  // no way to tell which filter will be more generic
  {
    base::RunLoop run_loop;
    adapter_->StartSessionWithFilter(std::move(discovery_filter3),
                                     run_loop.QuitClosure());
    run_loop.Run();
  }
  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  EXPECT_FALSE(resulting_filter->GetRSSI(&resulting_rssi));
  EXPECT_FALSE(resulting_filter->GetPathloss(&resulting_pathloss));

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, GetMergedDiscoveryFilterTransport) {
  std::unique_ptr<BluetoothDiscoveryFilter> resulting_filter;

  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_CLASSIC);

  auto discovery_filter2 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);

  adapter_->RunStartSessionWithFilter(std::move(discovery_filter));

  // Just one filter, make sure transport was properly rewritten
  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  EXPECT_EQ(BLUETOOTH_TRANSPORT_CLASSIC, resulting_filter->GetTransport());

  adapter_->RunStartSessionWithFilter(std::move(discovery_filter2));

  // Two filters, should have OR of both transport's
  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  EXPECT_EQ(BLUETOOTH_TRANSPORT_DUAL, resulting_filter->GetTransport());

  auto discovery_filter3 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter3->CopyFrom(
      BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_DUAL));

  // Merging empty filter in should result in empty filter
  adapter_->RunStartSessionWithFilter(std::move(discovery_filter3));
  resulting_filter = adapter_->GetMergedDiscoveryFilter();
  EXPECT_TRUE(resulting_filter->IsDefault());

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, GetMergedDiscoveryFilterAllFields) {
  int16_t resulting_rssi;
  std::set<device::BluetoothUUID> resulting_uuids;

  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-60);
  AddDeviceFilterWithUUID(discovery_filter.get(), BluetoothUUID("1000"));

  auto discovery_filter2 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter2->SetRSSI(-85);
  AddDeviceFilterWithUUID(discovery_filter2.get(), BluetoothUUID("1020"));
  AddDeviceFilterWithUUID(discovery_filter2.get(), BluetoothUUID("1001"));

  auto discovery_filter3 =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  discovery_filter3->SetRSSI(-65);
  discovery_filter3->SetTransport(BLUETOOTH_TRANSPORT_CLASSIC);
  AddDeviceFilterWithUUID(discovery_filter3.get(), BluetoothUUID("1020"));
  AddDeviceFilterWithUUID(discovery_filter3.get(), BluetoothUUID("1003"));

  adapter_->RunStartSessionWithFilter(std::move(discovery_filter));
  adapter_->RunStartSessionWithFilter(std::move(discovery_filter2));
  adapter_->RunStartSessionWithFilter(std::move(discovery_filter3));

  std::unique_ptr<BluetoothDiscoveryFilter> resulting_filter =
      adapter_->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  resulting_filter->GetUUIDs(resulting_uuids);
  EXPECT_TRUE(resulting_filter->GetTransport());
  EXPECT_EQ(BLUETOOTH_TRANSPORT_DUAL, resulting_filter->GetTransport());
  EXPECT_EQ(-85, resulting_rssi);
  EXPECT_THAT(resulting_uuids,
              testing::UnorderedElementsAre(device::BluetoothUUID("1000"),
                                            device::BluetoothUUID("1001"),
                                            device::BluetoothUUID("1003"),
                                            device::BluetoothUUID("1020")));

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, StartDiscoverySession_Destroy) {
  base::RunLoop loop;
  adapter_->StartDiscoverySession(
      /*client_name=*/std::string(),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<BluetoothDiscoverySession> session) {
            adapter_.reset();
            loop.Quit();
          }),
      base::DoNothing());
  loop.Run();
}

TEST_F(BluetoothAdapterTest, StartDiscoverySessionError_Destroy) {
  base::RunLoop loop;
  adapter_->set_discovery_session_outcome(
      UMABluetoothDiscoverySessionOutcome::FAILED);
  adapter_->StartDiscoverySession(/*client_name=*/std::string(),
                                  base::DoNothing(),
                                  base::BindLambdaForTesting([&]() {
                                    adapter_.reset();
                                    loop.Quit();
                                  }));
  loop.Run();
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
// TODO(https://crbug.com/331653043): Re-enable when passing on macOS 14 bots.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ConstructDefaultAdapter ConstructDefaultAdapter
#else
#define MAYBE_ConstructDefaultAdapter DISABLED_ConstructDefaultAdapter
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, ConstructDefaultAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructDefaultAdapter) {
#endif
  InitWithDefaultAdapter();
  if (!adapter_->IsPresent() || !adapter_->IsPowered()) {
    GTEST_SKIP()
        << "Bluetooth adapter not present or not powered; skipping unit test.";
  }

  EXPECT_FALSE(adapter_->GetAddress().empty());
  EXPECT_FALSE(adapter_->GetName().empty());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}  // namespace device

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_ConstructWithoutDefaultAdapter ConstructWithoutDefaultAdapter
#else
#define MAYBE_ConstructWithoutDefaultAdapter \
  DISABLED_ConstructWithoutDefaultAdapter
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, ConstructWithoutDefaultAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructWithoutDefaultAdapter) {
#endif  // BUILDFLAG(IS_WIN)
  InitWithoutDefaultAdapter();
  EXPECT_EQ(adapter_->GetAddress(), "");
  EXPECT_EQ(adapter_->GetName(), "");
#if !BUILDFLAG(IS_IOS)
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
#endif
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_ConstructFakeAdapter ConstructFakeAdapter
#else
#define MAYBE_ConstructFakeAdapter DISABLED_ConstructFakeAdapter
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, ConstructFakeAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructFakeAdapter) {
#endif  // BUILDFLAG(IS_WIN)
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
#if !BUILDFLAG(IS_IOS)
  EXPECT_EQ(adapter_->GetAddress(), kTestAdapterAddress);
  EXPECT_EQ(adapter_->GetName(), kTestAdapterName);
#endif
  EXPECT_TRUE(adapter_->CanPower());
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsPeripheralRoleSupported());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, ConstructFakeAdapterWithoutRadio) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitFakeAdapterWithoutRadio();
  EXPECT_EQ(adapter_->GetAddress(), kTestAdapterAddress);
  EXPECT_EQ(adapter_->GetName(), kTestAdapterName);
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->CanPower());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_P(BluetoothTestWinrt, ConstructFakeAdapterWithoutPowerControl) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitFakeAdapterWithRadioAccessDenied();
  EXPECT_EQ(adapter_->GetAddress(), kTestAdapterAddress);
  EXPECT_EQ(adapter_->GetName(), kTestAdapterName);
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->CanPower());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DiscoverySession DiscoverySession
#else
#define MAYBE_DiscoverySession DISABLED_DiscoverySession
#endif
// Starts and Stops a discovery session.
TEST_F(BluetoothTest, MAYBE_DiscoverySession) {
  InitWithFakeAdapter();
  EXPECT_FALSE(adapter_->IsDiscovering());

  StartLowEnergyDiscoverySession();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  ResetEventCounts();
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

// Android only: this test is specific for Android and should not be
// enabled for other platforms.
#if BUILDFLAG(IS_ANDROID)
TEST_F(BluetoothTest, AdapterIllegalStateBeforeStartScan) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  ForceIllegalStateException();
  StartLowEnergyDiscoverySessionExpectedToFail();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Android only: this test is specific for Android and should not be
// enabled for other platforms.
#if BUILDFLAG(IS_ANDROID)
TEST_F(BluetoothTest, AdapterIllegalStateBeforeStopScan) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ForceIllegalStateException();
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_NoPermissions NoPermissions
#else
#define MAYBE_NoPermissions DISABLED_NoPermissions
#endif
// Checks that discovery fails (instead of hanging) when permissions are denied.
TEST_F(BluetoothTest, MAYBE_NoPermissions) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  if (!DenyPermission()) {
    // Platform always gives permission to scan.
    return;
  }

  StartLowEnergyDiscoverySessionExpectedToFail();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
}

// Android-only: Only Android requires location services to be turned on to scan
// for Bluetooth devices.
#if BUILDFLAG(IS_ANDROID)
// Checks that discovery fails (instead of hanging) when location services are
// turned off.
TEST_F(BluetoothTest, NoLocationServices) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateLocationServicesOff();

  StartLowEnergyDiscoverySessionExpectedToFail();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_DiscoverLowEnergyDevice DiscoverLowEnergyDevice
#else
#define MAYBE_DiscoverLowEnergyDevice DISABLED_DiscoverLowEnergyDevice
#endif
// Discovers a device.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDevice) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDevice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery and find a device.
  StartLowEnergyDiscoverySession();
  SimulateLowEnergyDevice(1);
  EXPECT_EQ(1, observer.device_added_count());
  BluetoothDevice* device = adapter_->GetDevice(observer.last_device_address());
  EXPECT_TRUE(device);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_DiscoverLowEnergyDeviceTwice DiscoverLowEnergyDeviceTwice
#else
#define MAYBE_DiscoverLowEnergyDeviceTwice DISABLED_DiscoverLowEnergyDeviceTwice
#endif
// Discovers the same device multiple times.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDeviceTwice) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDeviceTwice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery and find a device.
  StartLowEnergyDiscoverySession();
  SimulateLowEnergyDevice(1);
  EXPECT_EQ(1, observer.device_added_count());
  BluetoothDevice* device = adapter_->GetDevice(observer.last_device_address());
  EXPECT_TRUE(device);

  // Find the same device again. This should not create a new device object.
  observer.Reset();
  SimulateLowEnergyDevice(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs \
  DiscoverLowEnergyDeviceWithUpdatedUUIDs
#else
#define MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs \
  DISABLED_DiscoverLowEnergyDeviceWithUpdatedUUIDs
#endif
// Discovers a device, and then again with new Service UUIDs.
// Makes sure we don't create another device when we've found the
// device in the past.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDeviceWithUpdatedUUIDs) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery and find a device.
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  // Discover same device again with updated UUIDs:
  observer.Reset();
  SimulateLowEnergyDevice(2);
  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  EXPECT_EQ(device, observer.last_device());

  // Discover same device again with empty UUIDs:
  observer.Reset();
  SimulateLowEnergyDevice(3);
  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_DiscoverMultipleLowEnergyDevices DiscoverMultipleLowEnergyDevices
#else
#define MAYBE_DiscoverMultipleLowEnergyDevices \
  DISABLED_DiscoverMultipleLowEnergyDevices
#endif
// Discovers multiple devices when addresses vary.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverMultipleLowEnergyDevices) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverMultipleLowEnergyDevices) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery and find a device.
  StartLowEnergyDiscoverySession();
  SimulateLowEnergyDevice(1);
  SimulateLowEnergyDevice(4);
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(2u, adapter_->GetDevices().size());
}

#if BUILDFLAG(IS_WIN)
// Tests that the adapter responds to external changes to the power state.
TEST_P(BluetoothTestWinrt, SimulateAdapterPoweredOffAndOn) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  SimulateAdapterPoweredOff();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  SimulateAdapterPoweredOn();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());
}

// Tests that power change notifications are deduplicated.
// Multiple StateChanged events with the same state only cause a
// single AdapterPoweredChanged() call.
TEST_P(BluetoothTestWinrt, SimulateDuplicateStateChanged) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  SimulateSpuriousRadioStateChangedEvent();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  SimulateAdapterPoweredOff();
  SimulateSpuriousRadioStateChangedEvent();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  SimulateAdapterPoweredOn();
  SimulateSpuriousRadioStateChangedEvent();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());
}

// Tests that the adapter responds to external changes to the power state, even
// if it failed to obtain the underlying radio.
TEST_P(BluetoothTestWinrt, SimulateAdapterPoweredOnAndOffWithoutRadio) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitFakeAdapterWithoutRadio();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  SimulateAdapterPoweredOn();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());

  SimulateAdapterPoweredOff();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());
}

// Makes sure the error callback gets run when changing the adapter power state
// fails.
// TODO(crbug.com/41410591): Implement SimulateAdapterPowerSuccess() and
// enable on all platforms.
TEST_P(BluetoothTestWinrt, SimulateAdapterPowerFailure) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());

  adapter_->SetPowered(false, GetCallback(Call::NOT_EXPECTED),
                       GetErrorCallback(Call::EXPECTED));
  SimulateAdapterPowerFailure();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->IsPowered());
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/41366193): Enable this test on old Windows versions as
// well.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, TogglePowerFakeAdapter) {
#else
#if BUILDFLAG(IS_IOS)
#define MAYBE_TogglePowerFakeAdapter DISABLED_TogglePowerFakeAdapter
#else
#define MAYBE_TogglePowerFakeAdapter TogglePowerFakeAdapter
#endif
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  // Check if power can be turned off.
  adapter_->SetPowered(false, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());

  // Check if power can be turned on again.
  adapter_->SetPowered(true, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_TogglePowerFakeAdapter_Twice TogglePowerFakeAdapter_Twice
#else
#define MAYBE_TogglePowerFakeAdapter_Twice DISABLED_TogglePowerFakeAdapter_Twice
#endif
// These tests are not relevant for BlueZ and old Windows versions. On these
// platforms the corresponding system APIs are blocking or use callbacks, so
// that it is not necessary to store pending callbacks and wait for the
// appropriate events.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, TogglePowerFakeAdapter_Twice) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  // Post two pending turn off requests, the second should fail due to the
  // presence of another callback.
  adapter_->SetPowered(false, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  adapter_->SetPowered(false, GetCallback(Call::NOT_EXPECTED),
                       GetErrorCallback(Call::EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());

  // Post two pending turn on requests, the second should fail due to the
  // presence of another callback.
  adapter_->SetPowered(true, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  adapter_->SetPowered(true, GetCallback(Call::NOT_EXPECTED),
                       GetErrorCallback(Call::EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off \
  TogglePowerFakeAdapter_WithinCallback_On_Off
#else
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off \
  DISABLED_TogglePowerFakeAdapter_WithinCallback_On_Off
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, TogglePowerFakeAdapter_WithinCallback_On_Off) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  // Turn adapter off, while powering it on in the callback.
  adapter_->SetPowered(false, base::BindLambdaForTesting([&] {
                         adapter_->SetPowered(
                             true, GetCallback(Call::EXPECTED),
                             GetErrorCallback(Call::NOT_EXPECTED));
                       }),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On \
  TogglePowerFakeAdapter_WithinCallback_Off_On
#else
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On \
  DISABLED_TogglePowerFakeAdapter_WithinCallback_Off_On
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, TogglePowerFakeAdapter_WithinCallback_Off_On) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  // Turn power off.
  adapter_->SetPowered(false, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());

  // Turn adapter on, while powering it off in the callback.
  adapter_->SetPowered(true, base::BindLambdaForTesting([&] {
                         adapter_->SetPowered(
                             false, GetCallback(Call::EXPECTED),
                             GetErrorCallback(Call::NOT_EXPECTED));
                       }),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(3, observer.powered_changed_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_TogglePowerFakeAdapter_DestroyWithPending \
  TogglePowerFakeAdapter_DestroyWithPending
#else
#define MAYBE_TogglePowerFakeAdapter_DestroyWithPending \
  DISABLED_TogglePowerFakeAdapter_DestroyWithPending
#endif

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, TogglePowerFakeAdapter_DestroyWithPending) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_DestroyWithPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());

  // Schedule pending power off request and cause destruction of the adapter by
  // dropping the reference count to 0. Note that we are intentionally not using
  // a TestBluetoothAdapterObserver, as this would hold another reference to the
  // adapter and thus interfere with the intended destruction.
  // We expect the error callback to be invoked, and any other subsequent calls
  // to SetPowered() should fail as well.
  bool error_callback_called = false;
  BluetoothAdapter* adapter = adapter_.get();
  adapter->SetPowered(
      false, GetCallback(Call::NOT_EXPECTED),
      base::BindLambdaForTesting(
          // Note that we explicitly need to capture a pointer to the
          // underlying adapter, even though we pass |this| implicitly. This is
          // because by the time this callback is invoked, |adapter| is already
          // set to null, but the pointed to adapter instance is still alive. So
          // using the pointer is safe, but dereferencing |adapter| crashes.
          [&] {
            error_callback_called = true;
            adapter->SetPowered(false, GetCallback(Call::NOT_EXPECTED),
                                GetErrorCallback(Call::EXPECTED));
            adapter->SetPowered(true, GetCallback(Call::NOT_EXPECTED),
                                GetErrorCallback(Call::EXPECTED));
          }));

  adapter_ = nullptr;
  // Empty the message loop to make sure posted callbacks get run.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(error_callback_called);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TogglePowerBeforeScan TogglePowerBeforeScan
#else
#define MAYBE_TogglePowerBeforeScan DISABLED_TogglePowerBeforeScan
#endif
TEST_F(BluetoothTest, MAYBE_TogglePowerBeforeScan) {
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count());

  // Turn off adapter.
  adapter_->SetPowered(false, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count());

  // Try to perform a scan.
  StartLowEnergyDiscoverySessionExpectedToFail();

  // Turn on adapter.
  adapter_->SetPowered(true, GetCallback(Call::EXPECTED),
                       GetErrorCallback(Call::NOT_EXPECTED));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count());

  // Try to perform a scan again.
  ResetEventCounts();
  StartLowEnergyDiscoverySession();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverySessionFailure) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  StartLowEnergyDiscoverySession();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  SimulateLowEnergyDiscoveryFailure();
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#define MAYBE_TurnOffAdapterWithConnectedDevice \
  TurnOffAdapterWithConnectedDevice
#else
#define MAYBE_TurnOffAdapterWithConnectedDevice \
  DISABLED_TurnOffAdapterWithConnectedDevice
#endif
// TODO(crbug.com/40522060): Enable on relevant platforms.
TEST_F(BluetoothTest, MAYBE_TurnOffAdapterWithConnectedDevice) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(device->IsGattConnected());

  ResetEventCounts();
  SimulateAdapterPoweredOff();

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, RegisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  InitWithFakeAdapter();
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_FALSE(pending_advertisements.empty());
  SimulateAdvertisementStarted(pending_advertisements[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, FailRegisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  InitWithFakeAdapter();
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::NOT_EXPECTED),
      GetAdvertisementErrorCallback(Call::EXPECTED));
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_FALSE(pending_advertisements.empty());
  SimulateAdvertisementError(pending_advertisements[0],
                             BluetoothAdvertisement::ERROR_ADAPTER_POWERED_OFF);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothAdvertisement::ERROR_ADAPTER_POWERED_OFF,
            last_advertisement_error_code_);
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, RegisterAndUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  InitWithFakeAdapter();
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_FALSE(pending_advertisements.empty());
  auto* advertisement = pending_advertisements[0];
  SimulateAdvertisementStarted(advertisement);
  base::RunLoop().RunUntilIdle();

  TestBluetoothAdvertisementObserver observer(advertisement);
  advertisement->Unregister(GetCallback(Call::EXPECTED),
                            GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  SimulateAdvertisementStopped(advertisement);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, FailUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  InitWithFakeAdapter();
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_FALSE(pending_advertisements.empty());
  auto* advertisement = pending_advertisements[0];
  SimulateAdvertisementStarted(advertisement);
  base::RunLoop().RunUntilIdle();

  TestBluetoothAdvertisementObserver observer(advertisement);
  advertisement->Unregister(GetCallback(Call::NOT_EXPECTED),
                            GetAdvertisementErrorCallback(Call::EXPECTED));
  SimulateAdvertisementError(advertisement,
                             BluetoothAdvertisement::ERROR_RESET_ADVERTISING);
  base::RunLoop().RunUntilIdle();

  // Expect no change to the observer status.
  EXPECT_FALSE(observer.released());
  EXPECT_EQ(0u, observer.released_count());
  EXPECT_EQ(BluetoothAdvertisement::ERROR_RESET_ADVERTISING,
            last_advertisement_error_code_);
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, RegisterAdvertisementWithInvalidData) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  // WinRT only accepts ManufacturerData in the payload, other data should be
  // rejected.
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_service_data(BluetoothAdvertisement::ServiceData());

  InitWithFakeAdapter();
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::NOT_EXPECTED),
      GetAdvertisementErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT,
            last_advertisement_error_code_);
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, RegisterMultipleAdvertisements) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  constexpr size_t kNumAdvertisements = 10u;

  for (size_t i = 0; i < kNumAdvertisements; ++i) {
    auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
        BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
    advertisement_data->set_manufacturer_data(
        BluetoothAdvertisement::ManufacturerData());

    adapter_->RegisterAdvertisement(
        std::move(advertisement_data),
        GetCreateAdvertisementCallback(Call::EXPECTED),
        GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  }

  base::RunLoop().RunUntilIdle();
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_EQ(kNumAdvertisements, pending_advertisements.size());
  for (size_t i = 0; i < kNumAdvertisements; ++i)
    SimulateAdvertisementStarted(pending_advertisements[i]);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, UnregisterAdvertisementWhilePendingUnregister) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));

  base::RunLoop().RunUntilIdle();
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_EQ(1u, pending_advertisements.size());
  auto* advertisement = pending_advertisements[0];
  SimulateAdvertisementStarted(advertisement);
  base::RunLoop().RunUntilIdle();

  TestBluetoothAdvertisementObserver observer(advertisement);
  advertisement->Unregister(GetCallback(Call::EXPECTED),
                            GetAdvertisementErrorCallback(Call::NOT_EXPECTED));

  // Schedule another Unregister, which is expected to fail.
  advertisement->Unregister(GetCallback(Call::NOT_EXPECTED),
                            GetAdvertisementErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();
  // Expect no change to the observer status.
  EXPECT_FALSE(observer.released());
  EXPECT_EQ(0u, observer.released_count());
  EXPECT_EQ(BluetoothAdvertisement::ERROR_RESET_ADVERTISING,
            last_advertisement_error_code_);

  // Simulate success of the first unregistration.
  SimulateAdvertisementStopped(advertisement);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, DoubleUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));

  base::RunLoop().RunUntilIdle();
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_EQ(1u, pending_advertisements.size());
  auto* advertisement = pending_advertisements[0];
  SimulateAdvertisementStarted(advertisement);
  base::RunLoop().RunUntilIdle();

  // Perform two unregistrations after each other. Both should succeed.
  TestBluetoothAdvertisementObserver observer(advertisement);
  advertisement->Unregister(GetCallback(Call::EXPECTED),
                            GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  SimulateAdvertisementStopped(advertisement);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());

  advertisement->Unregister(GetCallback(Call::EXPECTED),
                            GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  SimulateAdvertisementStopped(advertisement);
  base::RunLoop().RunUntilIdle();
  // The second unregister is a no-op, and should not notify observers again.
  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());

  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());
}

TEST_P(BluetoothTestWinrt, SimulateAdvertisementStoppedByOS) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      BluetoothAdvertisement::ManufacturerData());

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      GetCreateAdvertisementCallback(Call::EXPECTED),
      GetAdvertisementErrorCallback(Call::NOT_EXPECTED));

  base::RunLoop().RunUntilIdle();
  auto pending_advertisements = adapter_->GetPendingAdvertisementsForTesting();
  ASSERT_EQ(1u, pending_advertisements.size());
  auto* advertisement = pending_advertisements[0];
  SimulateAdvertisementStarted(advertisement);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->GetPendingAdvertisementsForTesting().empty());

  TestBluetoothAdvertisementObserver observer(advertisement);
  // Simulate the OS stopping the advertisement. This should notify the
  // |observer|.
  SimulateAdvertisementStopped(advertisement);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());

  // While Unregister() is a no-op now, we still expect an invocation of the
  // success callback, but no change to the |observer| state.
  advertisement->Unregister(GetCallback(Call::EXPECTED),
                            GetAdvertisementErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.released());
  EXPECT_EQ(1u, observer.released_count());
}

#endif  // BUILDFLAG(IS_WIN)

#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && \
    !defined(USE_CAST_BLUETOOTH_ADAPTER)
#define MAYBE_RegisterLocalGattServices RegisterLocalGattServices
#else
#define MAYBE_RegisterLocalGattServices DISABLED_RegisterLocalGattServices
#endif
TEST_F(BluetoothTest, MAYBE_RegisterLocalGattServices) {
  InitWithFakeAdapter();
  base::WeakPtr<BluetoothLocalGattService> service =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic1 =
      service->CreateCharacteristic(
          BluetoothUUID(kTestUUIDGenericAttribute),
          BluetoothLocalGattCharacteristic::Properties(),
          BluetoothLocalGattCharacteristic::Permissions());

  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic2 =
      service->CreateCharacteristic(
          BluetoothUUID(kTestUUIDGenericAttribute),
          BluetoothLocalGattCharacteristic::Properties(),
          BluetoothLocalGattCharacteristic::Permissions());

  base::WeakPtr<BluetoothLocalGattDescriptor> descriptor =
      BluetoothLocalGattDescriptor::Create(
          BluetoothUUID(kTestUUIDGenericAttribute),
          BluetoothLocalGattCharacteristic::Permissions(),
          characteristic1.get());

  service->Register(GetCallback(Call::EXPECTED),
                    GetGattErrorCallback(Call::NOT_EXPECTED));
  service->Register(GetCallback(Call::NOT_EXPECTED),
                    GetGattErrorCallback(Call::EXPECTED));
  service->Unregister(GetCallback(Call::EXPECTED),
                      GetGattErrorCallback(Call::NOT_EXPECTED));
  service->Unregister(GetCallback(Call::NOT_EXPECTED),
                      GetGattErrorCallback(Call::EXPECTED));
}

#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && \
    !defined(USE_CAST_BLUETOOTH_ADAPTER)
#define MAYBE_RegisterMultipleServices RegisterMultipleServices
#else
#define MAYBE_RegisterMultipleServices DISABLED_RegisterMultipleServices
#endif
TEST_F(BluetoothTest, MAYBE_RegisterMultipleServices) {
  InitWithFakeAdapter();
  base::WeakPtr<BluetoothLocalGattService> service2 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  base::WeakPtr<BluetoothLocalGattService> service3 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  base::WeakPtr<BluetoothLocalGattService> service4 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);

  service2->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service2.get()}));

  service3->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service2.get(), service3.get()}));

  service2->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service4->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service3.get(), service4.get()}));

  service3->Register(GetCallback(Call::NOT_EXPECTED),
                     GetGattErrorCallback(Call::EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service3.get(), service4.get()}));

  service3->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service4.get()}));

  service4->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));
}

#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && \
    !defined(USE_CAST_BLUETOOTH_ADAPTER)
#define MAYBE_DeleteServices DeleteServices
#else
#define MAYBE_DeleteServices DISABLED_DeleteServices
#endif
TEST_F(BluetoothTest, MAYBE_DeleteServices) {
  InitWithFakeAdapter();
  base::WeakPtr<BluetoothLocalGattService> service2 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  base::WeakPtr<BluetoothLocalGattService> service3 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  service2->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service2.get()}));

  service3->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service2.get(), service3.get()}));

  service2->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service2->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service3->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));

  // Create a service, register and then delete it, just to check everything
  // still works.
  base::WeakPtr<BluetoothLocalGattService> service4 =
      adapter_->CreateLocalGattService(BluetoothUUID(kTestUUIDGenericAttribute),
                                       true, nullptr);
  service4->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service4.get()}));
  service4->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));
}

// This test should only be enabled for platforms that uses the
// BluetoothAdapter#RemoveOutdatedDevices function to purge outdated
// devices.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_EnsureUpdatedTimestamps EnsureUpdatedTimestamps
#else
#define MAYBE_EnsureUpdatedTimestamps DISABLED_EnsureUpdatedTimestamps
#endif
TEST_F(BluetoothTest, MAYBE_EnsureUpdatedTimestamps) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Test that the timestamp of a device is updated during multiple
  // discovery sessions.
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(1, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  base::Time first_timestamp = device->GetLastUpdateTime();

  // Do a new discovery and check that the timestamp is updated.
  observer.Reset();
  StartLowEnergyDiscoverySession();
  SimulateLowEnergyDevice(1);
  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  base::Time second_timestamp = device->GetLastUpdateTime();
  EXPECT_TRUE(second_timestamp > first_timestamp);

  // Check that timestamp doesn't change when there is no discovery.
  base::Time third_timestamp = device->GetLastUpdateTime();
  EXPECT_TRUE(second_timestamp == third_timestamp);
}

// This test should only be enabled for platforms that uses the
// BluetoothAdapter#RemoveOutdatedDevices function to purge outdated
// devices.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_RemoveOutdatedDevices RemoveOutdatedDevices
#else
#define MAYBE_RemoveOutdatedDevices DISABLED_RemoveOutdatedDevices
#endif
TEST_F(BluetoothTest, MAYBE_RemoveOutdatedDevices) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device1 = SimulateLowEnergyDevice(1);
  BluetoothDevice* device2 = SimulateLowEnergyDevice(4);

  EXPECT_EQ(2u, adapter_->GetDevices().size());
  device1->SetAsExpiredForTesting();

  // Check that the outdated device is removed.
  RemoveTimedOutDevices();
  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  EXPECT_EQ(adapter_->GetDevices()[0]->GetAddress(), device2->GetAddress());
}

// This test should only be enabled for platforms that uses the
// BluetoothAdapter#RemoveOutdatedDevices function to purge outdated
// devices.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_RemoveOutdatedDeviceGattConnect RemoveOutdatedDeviceGattConnect
#else
#define MAYBE_RemoveOutdatedDeviceGattConnect \
  DISABLED_RemoveOutdatedDeviceGattConnect
#endif
TEST_F(BluetoothTest, MAYBE_RemoveOutdatedDeviceGattConnect) {
  // Test that a device with GATT connection isn't removed.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  device->SetAsExpiredForTesting();
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  RemoveTimedOutDevices();
  EXPECT_EQ(0, observer.device_removed_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}

#if BUILDFLAG(IS_APPLE)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with no service filter.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithNoFilter) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::GENERIC_DEVICE);
  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::HEART_RATE_DEVICE);
  BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet> result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(2u, result.size());
  for (const auto& pair : result) {
    EXPECT_TRUE(adapter_->GetDevice(pair.first->GetAddress()));
    EXPECT_TRUE(pair.second.empty());
  }
  EXPECT_EQ(BluetoothDevice::UUIDSet({BluetoothUUID(kTestUUIDGenericAccess)}),
            RetrieveConnectedPeripheralServiceUUIDs());
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(2u, adapter_->GetDevices().size());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithFilter) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::GENERIC_DEVICE);
  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::HEART_RATE_DEVICE);
  BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
  BluetoothUUID heart_service_uuid = BluetoothUUID(kTestUUIDHeartRate);
  AddDeviceFilterWithUUID(&discovery_filter, heart_service_uuid);
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet> result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(1u, result.size());
  for (const auto& pair : result) {
    EXPECT_EQ(kTestDeviceAddress2, pair.first->GetAddress());
    EXPECT_TRUE(adapter_->GetDevice(pair.first->GetAddress()));
    EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}), pair.second);
  }
  EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}),
            RetrieveConnectedPeripheralServiceUUIDs());
  EXPECT_EQ(1, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter
// that doesn't match.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithWrongFilter) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::GENERIC_DEVICE);
  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::HEART_RATE_DEVICE);
  BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(&discovery_filter, BluetoothUUID(kTestUUIDLinkLoss));
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet> result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(
      BluetoothDevice::UUIDSet({device::BluetoothUUID(kTestUUIDLinkLoss)}),
      RetrieveConnectedPeripheralServiceUUIDs());
  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(0u, adapter_->GetDevices().size());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with two service filters
// that both match.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithTwoFilters) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::GENERIC_DEVICE);
  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::HEART_RATE_DEVICE);
  BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
  BluetoothUUID heart_service_uuid = BluetoothUUID(kTestUUIDHeartRate);
  AddDeviceFilterWithUUID(&discovery_filter, heart_service_uuid);
  BluetoothUUID generic_service_uuid = BluetoothUUID(kTestUUIDGenericAccess);
  AddDeviceFilterWithUUID(&discovery_filter, generic_service_uuid);
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet> result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(2u, result.size());
  for (const auto& pair : result) {
    EXPECT_TRUE(adapter_->GetDevice(pair.first->GetAddress()));
    if (pair.first->GetAddress() == kTestDeviceAddress2) {
      EXPECT_EQ(
          BluetoothDevice::UUIDSet({heart_service_uuid, generic_service_uuid}),
          pair.second);
    } else if (pair.first->GetAddress() == kTestDeviceAddress1) {
      EXPECT_EQ(BluetoothDevice::UUIDSet({generic_service_uuid}), pair.second);
    } else {
      // Unknown device.
      EXPECT_TRUE(false);
    }
  }
  EXPECT_EQ(
      BluetoothDevice::UUIDSet({generic_service_uuid, heart_service_uuid}),
      RetrieveConnectedPeripheralServiceUUIDs());
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(2u, adapter_->GetDevices().size());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter
// that one match device, and then
// RetrieveGattConnectedDevicesWithDiscoveryFilter() again.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceTwice) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::GENERIC_DEVICE);
  SimulateConnectedLowEnergyDevice(ConnectedDeviceType::HEART_RATE_DEVICE);
  BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
  BluetoothUUID heart_service_uuid = BluetoothUUID(kTestUUIDHeartRate);
  AddDeviceFilterWithUUID(&discovery_filter, heart_service_uuid);
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet> result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(1u, result.size());
  for (const auto& pair : result) {
    EXPECT_EQ(kTestDeviceAddress2, pair.first->GetAddress());
    EXPECT_TRUE(adapter_->GetDevice(pair.first->GetAddress()));
    EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}), pair.second);
  }
  EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}),
            RetrieveConnectedPeripheralServiceUUIDs());
  EXPECT_EQ(1, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());

  observer.Reset();
  ResetRetrieveConnectedPeripheralServiceUUIDs();
  result = adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
      discovery_filter);

  EXPECT_EQ(1u, result.size());
  for (const auto& pair : result) {
    EXPECT_EQ(kTestDeviceAddress2, pair.first->GetAddress());
    EXPECT_TRUE(adapter_->GetDevice(pair.first->GetAddress()));
    EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}), pair.second);
  }
  EXPECT_EQ(BluetoothDevice::UUIDSet({heart_service_uuid}),
            RetrieveConnectedPeripheralServiceUUIDs());

  EXPECT_EQ(0, observer.device_added_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothTestWinrt,
                         ::testing::ValuesIn(kBluetoothTestWinrtParam));
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
