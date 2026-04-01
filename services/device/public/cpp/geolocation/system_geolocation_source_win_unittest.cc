// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_win.h"

#include <windows.security.authorization.appcapabilityaccess.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::ABI::Windows::Foundation::ITypedEventHandler;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapability;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapabilityAccessChangedEventArgs;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapabilityAccessStatus;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapabilityAccessStatus_Allowed;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapabilityAccessStatus_DeniedByUser;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    IAppCapability;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    IAppCapabilityAccessChangedEventArgs;
using ::Microsoft::WRL::ComPtr;
using ::Microsoft::WRL::InhibitRoOriginateError;
using ::Microsoft::WRL::RuntimeClass;
using ::Microsoft::WRL::RuntimeClassFlags;
using ::Microsoft::WRL::WinRtClassicComMix;

class FakeAppCapability
    : public RuntimeClass<
          RuntimeClassFlags<WinRtClassicComMix | InhibitRoOriginateError>,
          IAppCapability> {
 public:
  FakeAppCapability() = default;
  FakeAppCapability(const FakeAppCapability&) = delete;
  FakeAppCapability& operator=(const FakeAppCapability&) = delete;
  ~FakeAppCapability() override = default;

  IFACEMETHODIMP get_CapabilityName(HSTRING* value) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_User(ABI::Windows::System::IUser** value) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CheckAccess(AppCapabilityAccessStatus* status) override {
    check_access_count_++;
    *status = access_status_;
    return S_OK;
  }

  IFACEMETHODIMP add_AccessChanged(
      ITypedEventHandler<AppCapability*, AppCapabilityAccessChangedEventArgs*>*
          handler,
      EventRegistrationToken* token) override {
    add_access_changed_count_++;
    if (registration_hresult_ != S_OK) {
      return registration_hresult_;
    }
    handler_ = handler;
    token->value = 1;
    return S_OK;
  }

  IFACEMETHODIMP remove_AccessChanged(EventRegistrationToken token) override {
    remove_access_changed_count_++;
    handler_ = nullptr;
    return S_OK;
  }

  // Not used in tests.
  IFACEMETHODIMP RequestAccessAsync(
      ABI::Windows::Foundation::IAsyncOperation<AppCapabilityAccessStatus>**
          operation) override {
    return E_NOTIMPL;
  }

  void TriggerAccessChanged() {
    if (handler_) {
      handler_->Invoke(nullptr, nullptr);
    }
  }

  void set_access_status(AppCapabilityAccessStatus status) {
    access_status_ = status;
  }

  void set_registration_hresult(HRESULT hr) { registration_hresult_ = hr; }

  int check_access_count() const { return check_access_count_; }
  int add_access_changed_count() const { return add_access_changed_count_; }
  int remove_access_changed_count() const {
    return remove_access_changed_count_;
  }

 private:
  AppCapabilityAccessStatus access_status_ =
      AppCapabilityAccessStatus_DeniedByUser;
  HRESULT registration_hresult_ = S_OK;
  ComPtr<
      ITypedEventHandler<AppCapability*, AppCapabilityAccessChangedEventArgs*>>
      handler_;
  int check_access_count_ = 0;
  int add_access_changed_count_ = 0;
  int remove_access_changed_count_ = 0;
};

}  // namespace

class SystemGeolocationSourceWinTest : public testing::Test {
 public:
  SystemGeolocationSourceWinTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  SystemGeolocationSourceWinTest(const SystemGeolocationSourceWinTest&) =
      delete;
  SystemGeolocationSourceWinTest& operator=(
      const SystemGeolocationSourceWinTest&) = delete;
  ~SystemGeolocationSourceWinTest() override = default;

  void SetUp() override {
    fake_capability_ = Microsoft::WRL::Make<FakeAppCapability>();
    SystemGeolocationSourceWin::SetAppCapabilityFactoryForTesting(
        base::BindLambdaForTesting([&](std::string_view name) {
          return ComPtr<IAppCapability>(fake_capability_.Get());
        }));
  }

  void TearDown() override {
    SystemGeolocationSourceWin::SetAppCapabilityFactoryForTesting(
        base::NullCallback());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ComPtr<FakeAppCapability> fake_capability_;
};

TEST_F(SystemGeolocationSourceWinTest, PollingEnabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kWinSystemLocationPermissionEventBased);

  SystemGeolocationSourceWin source;
  base::RunLoop run_loop;
  source.RegisterPermissionUpdateCallback(base::BindLambdaForTesting(
      [&](LocationSystemPermissionStatus status) { run_loop.Quit(); }));
  run_loop.Run();

  // Initial check.
  EXPECT_EQ(fake_capability_->check_access_count(), 1);
  EXPECT_EQ(fake_capability_->add_access_changed_count(), 0);

  // Advance time to trigger a poll.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_GE(fake_capability_->check_access_count(), 2);
}

TEST_F(SystemGeolocationSourceWinTest, EventBasedEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kWinSystemLocationPermissionEventBased);

  SystemGeolocationSourceWin source;
  base::RunLoop run_loop;
  LocationSystemPermissionStatus last_status =
      LocationSystemPermissionStatus::kNotDetermined;
  source.RegisterPermissionUpdateCallback(
      base::BindLambdaForTesting([&](LocationSystemPermissionStatus status) {
        last_status = status;
        run_loop.Quit();
      }));
  run_loop.Run();

  // Initial check + registration.
  EXPECT_EQ(fake_capability_->check_access_count(), 1);
  EXPECT_EQ(fake_capability_->add_access_changed_count(), 1);

  // Trigger event and verify update.
  fake_capability_->set_access_status(AppCapabilityAccessStatus_Allowed);
  fake_capability_->TriggerAccessChanged();

  base::RunLoop run_loop2;
  source.RegisterPermissionUpdateCallback(
      base::BindLambdaForTesting([&](LocationSystemPermissionStatus status) {
        if (status == LocationSystemPermissionStatus::kAllowed) {
          run_loop2.Quit();
        }
      }));
  run_loop2.Run();
  EXPECT_EQ(fake_capability_->check_access_count(), 2);

  // Verify no polling is happening.
  int count_before = fake_capability_->check_access_count();
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(fake_capability_->check_access_count(), count_before);
}

TEST_F(SystemGeolocationSourceWinTest, EventBasedFallbackToPolling) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kWinSystemLocationPermissionEventBased);
  fake_capability_->set_registration_hresult(E_FAIL);

  SystemGeolocationSourceWin source;
  base::RunLoop run_loop;
  source.RegisterPermissionUpdateCallback(base::BindLambdaForTesting(
      [&](LocationSystemPermissionStatus status) { run_loop.Quit(); }));
  run_loop.Run();

  // Failed registration should trigger fallback to polling.
  EXPECT_EQ(fake_capability_->add_access_changed_count(), 1);
  EXPECT_EQ(fake_capability_->check_access_count(), 1);

  // Advance time to trigger a poll.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_GE(fake_capability_->check_access_count(), 2);
}

}  // namespace device
