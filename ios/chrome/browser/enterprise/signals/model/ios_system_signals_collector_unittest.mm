// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_system_signals_collector.h"

#import <memory>
#import <unordered_set>
#import <vector>

#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/common/ui/reauthentication/fake_reauthentication_module.h"
#import "ios/chrome/test/providers/signin/test_device_identifier.h"
#import "ios/public/provider/chrome/browser/signin/device_identifier_api.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kFakeVendorId[] = "fake-vendor-id";
constexpr char kFakeDeviceAffiliationId[] = "device-affiliation-id";
constexpr char kFakeDeviceEnrollmentDomain[] = "device.domain.com";

constexpr char kCollectionSuccessHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success";
constexpr char kCollectionFailureHistogram[] =
    "Enterprise.DeviceSignals.Collection.Failure";
constexpr char kOsSignalsSuccessLatencyHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success.OsSignals.Latency";

class IOSSystemSignalsCollectorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ios::provider::test::SetDeviceIdentifier(kFakeVendorId);
    reauth_module_ = [[FakeReauthenticationModule alloc] init];

    // Stands in for the browser-level (CBCM) policy manager; only the policy
    // store is read by the collector.
    enterprise_management::PolicyData device_policy_data;
    device_policy_data.set_managed_by(kFakeDeviceEnrollmentDomain);
    auto store = std::make_unique<policy::MockCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    store->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(device_policy_data)));
    device_cloud_policy_manager_ =
        std::make_unique<policy::MockCloudPolicyManager>(
            std::move(store), task_environment_.GetMainThreadTaskRunner());

    collector_ = CreateCollector(device_cloud_policy_manager_.get());
  }

  void TearDown() override {
    collector_.reset();
    device_cloud_policy_manager_.reset();
    ios::provider::test::ResetDeviceIdentifier();
    PlatformTest::TearDown();
  }

  std::unique_ptr<IOSSystemSignalsCollector> CreateCollector(
      policy::CloudPolicyManager* device_cloud_policy_manager) {
    return std::make_unique<IOSSystemSignalsCollector>(
        base::BindRepeating([] {
          return base::flat_set<std::string>{kFakeDeviceAffiliationId};
        }),
        device_cloud_policy_manager, reauth_module_);
  }

  device_signals::OsSignalsResponse CollectOsSignals(
      IOSSystemSignalsCollector* collector) {
    device_signals::SignalsAggregationRequest request;
    device_signals::SignalsAggregationResponse response;
    base::RunLoop run_loop;
    collector->GetSignal(device_signals::SignalName::kOsSignals,
                         device_signals::UserPermission::kGranted, request,
                         response, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(response.os_signals_response.has_value());
    return response.os_signals_response.value_or(
        device_signals::OsSignalsResponse());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyManager> device_cloud_policy_manager_;
  std::unique_ptr<IOSSystemSignalsCollector> collector_;

  FakeReauthenticationModule* reauth_module_;
};

TEST_F(IOSSystemSignalsCollectorTest, GetSupportedSignalNames) {
  const std::unordered_set<device_signals::SignalName> supported_signals =
      collector_->GetSupportedSignalNames();
  EXPECT_EQ(supported_signals.size(), 1u);
  EXPECT_TRUE(supported_signals.find(device_signals::SignalName::kOsSignals) !=
              supported_signals.end());
}

TEST_F(IOSSystemSignalsCollectorTest, IsSignalSupported) {
  EXPECT_TRUE(
      collector_->IsSignalSupported(device_signals::SignalName::kOsSignals));
  EXPECT_FALSE(collector_->IsSignalSupported(
      device_signals::SignalName::kBrowserContextSignals));
}

TEST_F(IOSSystemSignalsCollectorTest, GetOsSignals_Success) {
  reauth_module_.canAttempt = YES;
  base::HistogramTester histogram_tester;

  const device_signals::OsSignalsResponse os_signals =
      CollectOsSignals(collector_.get());
  EXPECT_EQ(os_signals.operating_system, "iOS");
  EXPECT_EQ(os_signals.vendor_id, kFakeVendorId);
  EXPECT_EQ(os_signals.browser_version, version_info::GetVersionNumber());
  EXPECT_EQ(os_signals.device_affiliation_ids,
            std::vector<std::string>({kFakeDeviceAffiliationId}));
  EXPECT_EQ(os_signals.device_enrollment_domain, kFakeDeviceEnrollmentDomain);
  EXPECT_EQ(os_signals.screen_lock_secured,
            device_signals::SettingValue::ENABLED);
  EXPECT_EQ(os_signals.disk_encryption,
            device_signals::SettingValue::ENABLED);  // Mirrors screen lock
  EXPECT_EQ(os_signals.os_firewall, device_signals::SettingValue::UNKNOWN);

  // The collection outcome is recorded as a success, along with the latency
  // measured from the start of the collection to its outcome.
  histogram_tester.ExpectUniqueSample(
      kCollectionSuccessHistogram, device_signals::SignalName::kOsSignals, 1);
  histogram_tester.ExpectTotalCount(kOsSignalsSuccessLatencyHistogram, 1);
  histogram_tester.ExpectTotalCount(kCollectionFailureHistogram, 0);
}

TEST_F(IOSSystemSignalsCollectorTest, GetOsSignals_ScreenLockDisabled) {
  reauth_module_.canAttempt = NO;

  const device_signals::OsSignalsResponse os_signals =
      CollectOsSignals(collector_.get());
  EXPECT_EQ(os_signals.screen_lock_secured,
            device_signals::SettingValue::DISABLED);
  EXPECT_EQ(os_signals.disk_encryption, device_signals::SettingValue::DISABLED);
}

// Browser not enrolled in CBCM: no machine-level policy manager exists.
TEST_F(IOSSystemSignalsCollectorTest, GetOsSignals_NoDevicePolicyManager) {
  reauth_module_.canAttempt = YES;
  std::unique_ptr<IOSSystemSignalsCollector> collector =
      CreateCollector(/*device_cloud_policy_manager=*/nullptr);

  const device_signals::OsSignalsResponse os_signals =
      CollectOsSignals(collector.get());
  EXPECT_FALSE(os_signals.device_enrollment_domain.has_value());
  // Unrelated signals are still reported.
  EXPECT_EQ(os_signals.device_affiliation_ids,
            std::vector<std::string>({kFakeDeviceAffiliationId}));
}

// The manager is handed to the collector before CBCM policy is fetched, so the
// collector must read the store at collection time instead of snapshotting it.
TEST_F(IOSSystemSignalsCollectorTest,
       GetOsSignals_PolicyLoadedAfterCollectorCreation) {
  reauth_module_.canAttempt = YES;

  auto store = std::make_unique<policy::MockCloudPolicyStore>(
      policy::dm_protocol::GetChromeUserPolicyType());
  policy::MockCloudPolicyStore* store_ptr = store.get();
  auto manager = std::make_unique<policy::MockCloudPolicyManager>(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  std::unique_ptr<IOSSystemSignalsCollector> collector =
      CreateCollector(manager.get());

  EXPECT_FALSE(
      CollectOsSignals(collector.get()).device_enrollment_domain.has_value());

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(kFakeDeviceEnrollmentDomain);
  store_ptr->set_policy_data_for_testing(std::move(policy_data));

  EXPECT_EQ(CollectOsSignals(collector.get()).device_enrollment_domain,
            kFakeDeviceEnrollmentDomain);
}

}  // namespace
