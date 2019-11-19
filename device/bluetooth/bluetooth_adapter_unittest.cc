// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
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
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif defined(OS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

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

  std::string GetAddress() const override { return ""; }

  std::string GetName() const override { return ""; }

  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override {}

  bool IsInitialized() const override { return false; }

  bool IsPresent() const override { return false; }

  bool IsPowered() const override { return false; }

  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {}

  bool IsDiscoverable() const override { return false; }

  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {}

  bool IsDiscovering() const override { return false; }

  UUIDList GetUUIDs() const override { return UUIDList(); }

  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override {}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override {}
  void ResetAdvertising(
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override {}
#endif

  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override {
    return nullptr;
  }

  void TestErrorCallback() {}

  void TestOnStartDiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_holder_.push(std::move(discovery_session));
  }

  void OnStartDiscoverySessionQuitLoop(
      base::Closure run_loop_quit,
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    run_loop_quit.Run();
    discovery_sessions_holder_.push(std::move(discovery_session));
  }

  void OnRemoveDiscoverySession(base::Closure run_loop_quit) {
    ++callback_count_;
    run_loop_quit.Run();
  }

  void OnRemoveDiscoverySessionError(base::Closure run_loop_quit) {
    ++callback_count_;
    run_loop_quit.Run();
  }

  void set_discovery_session_outcome(
      UMABluetoothDiscoverySessionOutcome outcome) {
    discovery_session_outcome_ = outcome;
  }

  void StopDiscoverySession(base::Closure run_loop_quit) {
    discovery_sessions_holder_.front()->Stop(
        base::BindRepeating(&TestBluetoothAdapter::OnRemoveDiscoverySession,
                            this, run_loop_quit),
        base::BindRepeating(
            &TestBluetoothAdapter::OnRemoveDiscoverySessionError, this,
            run_loop_quit));
    discovery_sessions_holder_.pop();
  }

  void StopAllDiscoverySessions(base::Closure run_loop_quit) {
    int num_stop_requests = discovery_sessions_holder_.size();
    while (!discovery_sessions_holder_.empty()) {
      discovery_sessions_holder_.front()->Stop(
          base::BindLambdaForTesting(
              [run_loop_quit, num_stop_requests, this]() {
                num_requests_returned_++;
                ++callback_count_;
                if (num_requests_returned_ == num_stop_requests) {
                  num_requests_returned_ = 0;
                  run_loop_quit.Run();
                }
              }),
          base::BindRepeating(
              &TestBluetoothAdapter::OnRemoveDiscoverySessionError, this,
              run_loop_quit));
      discovery_sessions_holder_.pop();
    }
  }

  void CleanupSessions() {
    // clear discovery_sessions_holder_
    base::queue<std::unique_ptr<BluetoothDiscoverySession>> empty_queue;
    std::swap(discovery_sessions_holder_, empty_queue);
  }

  void QueueStartRequests(base::Closure run_loop_quit, int num_requests) {
    for (int i = 0; i < num_requests; ++i) {
      StartDiscoverySession(
          base::BindLambdaForTesting(
              [run_loop_quit, num_requests,
               this](std::unique_ptr<device::BluetoothDiscoverySession>
                         discovery_session) {
                ++callback_count_;
                num_requests_returned_++;
                discovery_sessions_holder_.push(std::move(discovery_session));
                if (num_requests_returned_ == num_requests) {
                  num_requests_returned_ = 0;
                  run_loop_quit.Run();
                }
              }),
          base::Bind(&TestBluetoothAdapter::TestErrorCallback, this));
    };
  }

  void StartSessionWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      base::RepeatingClosure run_loop_quit) {
    StartDiscoverySessionWithFilter(
        std::move(discovery_filter),
        base::Bind(&TestBluetoothAdapter::OnStartDiscoverySessionQuitLoop, this,
                   run_loop_quit),
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestBluetoothAdapter::SetFilter,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(discovery_filter), std::move(callback)));
  }

  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  int num_requests_returned_ = 0;

 private:
  void PostDelayedTask(base::OnceClosure callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
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
  EXPECT_EQ(4UL, resulting_uuids.size());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1000")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1001")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1003")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1020")) !=
              resulting_uuids.end());

  adapter_->CleanupSessions();
}

TEST_F(BluetoothAdapterTest, StartDiscoverySession_Destroy) {
  base::RunLoop loop;
  adapter_->StartDiscoverySession(
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
  adapter_->StartDiscoverySession(base::DoNothing(),
                                  base::BindLambdaForTesting([&]() {
                                    adapter_.reset();
                                    loop.Quit();
                                  }));
  loop.Run();
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ConstructDefaultAdapter ConstructDefaultAdapter
#else
#define MAYBE_ConstructDefaultAdapter DISABLED_ConstructDefaultAdapter
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, ConstructDefaultAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructDefaultAdapter) {
#endif
  InitWithDefaultAdapter();
  if (!adapter_->IsPresent()) {
    LOG(WARNING) << "Bluetooth adapter not present; skipping unit test.";
    return;
  }

  bool expected = false;
// MacOS returns empty for name and address if the adapter is off.
#if defined(OS_MACOSX)
  expected = !adapter_->IsPowered();
#endif  // defined(OS_MACOSX)

  EXPECT_EQ(expected, adapter_->GetAddress().empty());
  EXPECT_EQ(expected, adapter_->GetName().empty());

  EXPECT_TRUE(adapter_->IsPresent());
  // Don't know on test machines if adapter will be powered or not, but
  // the call should be safe to make and consistent.
  EXPECT_EQ(adapter_->IsPowered(), adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ConstructWithoutDefaultAdapter ConstructWithoutDefaultAdapter
#else
#define MAYBE_ConstructWithoutDefaultAdapter \
  DISABLED_ConstructWithoutDefaultAdapter
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, ConstructWithoutDefaultAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructWithoutDefaultAdapter) {
#endif  // defined(OS_WIN)
  InitWithoutDefaultAdapter();
  EXPECT_EQ(adapter_->GetAddress(), "");
  EXPECT_EQ(adapter_->GetName(), "");
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ConstructFakeAdapter ConstructFakeAdapter
#else
#define MAYBE_ConstructFakeAdapter DISABLED_ConstructFakeAdapter
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, ConstructFakeAdapter) {
#else
TEST_F(BluetoothTest, MAYBE_ConstructFakeAdapter) {
#endif  // defined(OS_WIN)
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  EXPECT_EQ(adapter_->GetAddress(), kTestAdapterAddress);
  EXPECT_EQ(adapter_->GetName(), kTestAdapterName);
  EXPECT_TRUE(adapter_->CanPower());
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, ConstructFakeAdapterWithoutRadio) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_WIN)

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
TEST_F(BluetoothTest, AdapterIllegalStateBeforeStartScan) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  ForceIllegalStateException();
  StartLowEnergyDiscoverySessionExpectedToFail();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // defined(OS_ANDROID)

// Android only: this test is specific for Android and should not be
// enabled for other platforms.
#if defined(OS_ANDROID)
TEST_F(BluetoothTest, AdapterIllegalStateBeforeStopScan) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_NoPermissions NoPermissions
#else
#define MAYBE_NoPermissions DISABLED_NoPermissions
#endif
// Checks that discovery fails (instead of hanging) when permissions are denied.
TEST_F(BluetoothTest, MAYBE_NoPermissions) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#if defined(OS_ANDROID)
// Checks that discovery fails (instead of hanging) when location services are
// turned off.
TEST_F(BluetoothTest, NoLocationServices) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateLocationServicesOff();

  StartLowEnergyDiscoverySessionExpectedToFail();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_DiscoverLowEnergyDevice DiscoverLowEnergyDevice
#else
#define MAYBE_DiscoverLowEnergyDevice DISABLED_DiscoverLowEnergyDevice
#endif
// Discovers a device.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDevice) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDevice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_DiscoverLowEnergyDeviceTwice DiscoverLowEnergyDeviceTwice
#else
#define MAYBE_DiscoverLowEnergyDeviceTwice DISABLED_DiscoverLowEnergyDeviceTwice
#endif
// Discovers the same device multiple times.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDeviceTwice) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDeviceTwice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs \
  DiscoverLowEnergyDeviceWithUpdatedUUIDs
#else
#define MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs \
  DISABLED_DiscoverLowEnergyDeviceWithUpdatedUUIDs
#endif
// Discovers a device, and then again with new Service UUIDs.
// Makes sure we don't create another device when we've found the
// device in the past.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverLowEnergyDeviceWithUpdatedUUIDs) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverLowEnergyDeviceWithUpdatedUUIDs) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_DiscoverMultipleLowEnergyDevices DiscoverMultipleLowEnergyDevices
#else
#define MAYBE_DiscoverMultipleLowEnergyDevices \
  DISABLED_DiscoverMultipleLowEnergyDevices
#endif
// Discovers multiple devices when addresses vary.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrt, DiscoverMultipleLowEnergyDevices) {
#else
TEST_F(BluetoothTest, MAYBE_DiscoverMultipleLowEnergyDevices) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_WIN)
// Tests that the adapter responds to external changes to the power state.
TEST_P(BluetoothTestWinrtOnly, SimulateAdapterPoweredOffAndOn) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

// Tests that the adapter responds to external changes to the power state, even
// if it failed to obtain the underlying radio.
TEST_P(BluetoothTestWinrtOnly, SimulateAdapterPoweredOnAndOffWithoutRadio) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
// TODO(https://crbug.com/878680): Implement SimulateAdapterPowerSuccess() and
// enable on all platforms.
TEST_P(BluetoothTestWinrtOnly, SimulateAdapterPowerFailure) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_WIN)

// TODO(https://crbug.com/804356): Enable this test on old Windows versions as
// well.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, TogglePowerFakeAdapter) {
#else
TEST_F(BluetoothTest, TogglePowerFakeAdapter) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_TogglePowerFakeAdapter_Twice TogglePowerFakeAdapter_Twice
#else
#define MAYBE_TogglePowerFakeAdapter_Twice DISABLED_TogglePowerFakeAdapter_Twice
#endif
// These tests are not relevant for BlueZ and old Windows versions. On these
// platforms the corresponding system APIs are blocking or use callbacks, so
// that it is not necessary to store pending callbacks and wait for the
// appropriate events.
#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, TogglePowerFakeAdapter_Twice) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off \
  TogglePowerFakeAdapter_WithinCallback_On_Off
#else
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off \
  DISABLED_TogglePowerFakeAdapter_WithinCallback_On_Off
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, TogglePowerFakeAdapter_WithinCallback_On_Off) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_WithinCallback_On_Off) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On \
  TogglePowerFakeAdapter_WithinCallback_Off_On
#else
#define MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On \
  DISABLED_TogglePowerFakeAdapter_WithinCallback_Off_On
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, TogglePowerFakeAdapter_WithinCallback_Off_On) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_WithinCallback_Off_On) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_TogglePowerFakeAdapter_DestroyWithPending \
  TogglePowerFakeAdapter_DestroyWithPending
#else
#define MAYBE_TogglePowerFakeAdapter_DestroyWithPending \
  DISABLED_TogglePowerFakeAdapter_DestroyWithPending
#endif

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, TogglePowerFakeAdapter_DestroyWithPending) {
#else
TEST_F(BluetoothTest, MAYBE_TogglePowerFakeAdapter_DestroyWithPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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

#if defined(OS_ANDROID)
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

#if defined(OS_MACOSX)
#define MAYBE_TurnOffAdapterWithConnectedDevice \
  TurnOffAdapterWithConnectedDevice
#else
#define MAYBE_TurnOffAdapterWithConnectedDevice \
  DISABLED_TurnOffAdapterWithConnectedDevice
#endif
// TODO(crbug.com/725270): Enable on relevant platforms.
TEST_F(BluetoothTest, MAYBE_TurnOffAdapterWithConnectedDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(device->IsGattConnected());

  ResetEventCounts();
  SimulateAdapterPoweredOff();

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
}

#if defined(OS_WIN)
TEST_P(BluetoothTestWinrtOnly, RegisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, FailRegisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, RegisterAndUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, FailUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, RegisterAdvertisementWithInvalidData) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  // WinRT only accepts ManufacturerData in the payload, other data should be
  // rejected.
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_service_data(
      std::make_unique<BluetoothAdvertisement::ServiceData>());

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

TEST_P(BluetoothTestWinrtOnly, RegisterMultipleAdvertisements) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  constexpr size_t kNumAdvertisements = 10u;

  for (size_t i = 0; i < kNumAdvertisements; ++i) {
    auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
        BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
    advertisement_data->set_manufacturer_data(
        std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, UnregisterAdvertisementWhilePendingUnregister) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, DoubleUnregisterAdvertisement) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

TEST_P(BluetoothTestWinrtOnly, SimulateAdvertisementStoppedByOS) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>());

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

#endif  // defined(OS_WIN)

#if (defined(OS_CHROMEOS) || defined(OS_LINUX)) && \
    !defined(USE_CAST_BLUETOOTH_ADAPTER)
#define MAYBE_RegisterLocalGattServices RegisterLocalGattServices
#else
#define MAYBE_RegisterLocalGattServices DISABLED_RegisterLocalGattServices
#endif
TEST_F(BluetoothTest, MAYBE_RegisterLocalGattServices) {
  InitWithFakeAdapter();
  base::WeakPtr<BluetoothLocalGattService> service =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, nullptr);
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic1 =
      BluetoothLocalGattCharacteristic::Create(
          BluetoothUUID(kTestUUIDGenericAttribute),
          BluetoothLocalGattCharacteristic::Properties(),
          BluetoothLocalGattCharacteristic::Permissions(), service.get());

  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic2 =
      BluetoothLocalGattCharacteristic::Create(
          BluetoothUUID(kTestUUIDGenericAttribute),
          BluetoothLocalGattCharacteristic::Properties(),
          BluetoothLocalGattCharacteristic::Permissions(), service.get());

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

// This test should only be enabled for platforms that uses the
// BluetoothAdapter#RemoveOutdatedDevices function to purge outdated
// devices.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_EnsureUpdatedTimestamps EnsureUpdatedTimestamps
#else
#define MAYBE_EnsureUpdatedTimestamps DISABLED_EnsureUpdatedTimestamps
#endif
TEST_F(BluetoothTest, MAYBE_EnsureUpdatedTimestamps) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoveOutdatedDevices RemoveOutdatedDevices
#else
#define MAYBE_RemoveOutdatedDevices DISABLED_RemoveOutdatedDevices
#endif
TEST_F(BluetoothTest, MAYBE_RemoveOutdatedDevices) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoveOutdatedDeviceGattConnect RemoveOutdatedDeviceGattConnect
#else
#define MAYBE_RemoveOutdatedDeviceGattConnect \
  DISABLED_RemoveOutdatedDeviceGattConnect
#endif
TEST_F(BluetoothTest, MAYBE_RemoveOutdatedDeviceGattConnect) {
  // Test that a device with GATT connection isn't removed.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  device->SetAsExpiredForTesting();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, adapter_->GetDevices().size());
  RemoveTimedOutDevices();
  EXPECT_EQ(0, observer.device_removed_count());
  EXPECT_EQ(1u, adapter_->GetDevices().size());
}

#if defined(OS_MACOSX)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with no service filter.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithNoFilter) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithFilter) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter
// that doesn't match.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithWrongFilter) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with two service filters
// that both match.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceWithTwoFilters) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Simulate two devices being connected before calling
// RetrieveGattConnectedDevicesWithDiscoveryFilter() with one service filter
// that one match device, and then
// RetrieveGattConnectedDevicesWithDiscoveryFilter() again.
TEST_F(BluetoothTest, DiscoverConnectedLowEnergyDeviceTwice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
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
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BluetoothTestWinrt,
    ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BluetoothTestWinrtOnly,
    ::testing::Values(true));
#endif  // defined(OS_WIN)

}  // namespace device
