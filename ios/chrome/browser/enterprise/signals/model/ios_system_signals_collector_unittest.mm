// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_system_signals_collector.h"

#import <memory>
#import <unordered_set>

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_device_identifier_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

constexpr char kFakeVendorId[] = "fake-vendor-id";

class MockIOSDeviceIdentifier : public IOSDeviceIdentifierDelegate {
 public:
  MOCK_METHOD(std::string, GetVendorId, (), (override));
};

class IOSSystemSignalsCollectorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    auto mock_delegate = std::make_unique<MockIOSDeviceIdentifier>();
    mock_delegate_ptr_ = mock_delegate.get();
    // Default behavior for vendor ID.
    ON_CALL(*mock_delegate_ptr_, GetVendorId())
        .WillByDefault(testing::Return(kFakeVendorId));
    collector_ =
        std::make_unique<IOSSystemSignalsCollector>(std::move(mock_delegate));

    mock_auth_module_ = OCMClassMock([ReauthenticationModule class]);

    OCMStub([mock_auth_module_ alloc]).andReturn(mock_auth_module_);
  }

  void TearDown() override {
    [mock_auth_module_ stopMocking];
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IOSSystemSignalsCollector> collector_;
  // The delegate is owned by the collector, therefore `mock_delegate_ptr_` must
  // be cleaned up before collector_ is destroyed to avoid a dangling pointer.
  raw_ptr<MockIOSDeviceIdentifier> mock_delegate_ptr_;
  id mock_auth_module_;
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
  // Mock canAttemptReauth to return YES (Secured).
  OCMStub([mock_auth_module_ canAttemptReauth]).andReturn(YES);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;
  collector_->GetSignal(device_signals::SignalName::kOsSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(response.os_signals_response.has_value());
  const device_signals::OsSignalsResponse& os_signals =
      response.os_signals_response.value();
  EXPECT_EQ(os_signals.operating_system, "iOS");
  EXPECT_EQ(os_signals.vendor_id, kFakeVendorId);
  EXPECT_EQ(os_signals.browser_version, version_info::GetVersionNumber());
  EXPECT_EQ(os_signals.screen_lock_secured,
            device_signals::SettingValue::ENABLED);
  EXPECT_EQ(os_signals.disk_encryption,
            device_signals::SettingValue::ENABLED);  // Mirrors screen lock
  EXPECT_EQ(os_signals.os_firewall, device_signals::SettingValue::UNKNOWN);
}

TEST_F(IOSSystemSignalsCollectorTest, GetOsSignals_ScreenLockDisabled) {
  // Mock canAttemptReauth to return NO.
  OCMStub([mock_auth_module_ canAttemptReauth]).andReturn(NO);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;
  collector_->GetSignal(device_signals::SignalName::kOsSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(response.os_signals_response.has_value());
  const device_signals::OsSignalsResponse& os_signals =
      response.os_signals_response.value();
  EXPECT_EQ(os_signals.screen_lock_secured,
            device_signals::SettingValue::DISABLED);
  EXPECT_EQ(os_signals.disk_encryption, device_signals::SettingValue::DISABLED);
}

}  // namespace
