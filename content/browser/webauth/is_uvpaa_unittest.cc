// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/is_uvpaa.h"

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "device/fido/public/features.h"
#else
#include "base/test/task_environment.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_MAC)

class FakeWebAuthenticationDelegate : public WebAuthenticationDelegate {
 public:
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      BrowserContext* browser_context) override {
    last_browser_context = browser_context;
    return touch_id_config;
  }

  std::optional<TouchIdAuthenticatorConfig> touch_id_config;
  raw_ptr<BrowserContext> last_browser_context = nullptr;
};

class FakeContentBrowserClient : public ContentBrowserClient {
 public:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &delegate;
  }

  FakeWebAuthenticationDelegate delegate;
};

class IsUVPAAMacTest : public testing::Test {
 protected:
  void SetUp() override {
    old_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override {
    browser_client_.delegate.last_browser_context = nullptr;
    SetBrowserClientForTesting(old_client_);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      /*enabled_features=*/{},
      /*disabled_features=*/{
          device::kWebAuthnICloudKeychainUseDeviceConfiguredForPasskeys}};
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  FakeContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

TEST_F(IsUVPAAMacTest, NullTouchIdConfigReturnsFalse) {
  browser_client_.delegate.touch_id_config = std::nullopt;

  base::test::TestFuture<bool> future;
  IsUVPlatformAuthenticatorAvailable(&browser_context_, future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_EQ(browser_client_.delegate.last_browser_context, &browser_context_);
}

TEST_F(IsUVPAAMacTest, TouchIdAvailability) {
  const device::fido::mac::AuthenticatorConfig config{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  browser_client_.delegate.touch_id_config = config;
  device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment(
      config);

  for (const bool available : {false, true}) {
    touch_id_test_environment.SetTouchIdAvailable(available);
    base::test::TestFuture<bool> future;
    IsUVPlatformAuthenticatorAvailable(&browser_context_, future.GetCallback());
    EXPECT_EQ(future.Get(), available);
    EXPECT_EQ(browser_client_.delegate.last_browser_context, &browser_context_);
  }
}

#elif BUILDFLAG(IS_WIN)

TEST(IsUVPAATest, WinWebAuthnApi) {
  base::test::TaskEnvironment task_environment;
  device::FakeWinWebAuthnApi fake_api;
  device::WinWebAuthnApi::ScopedOverride override(&fake_api);

  fake_api.set_available(false);
  fake_api.set_is_uvpaa(true);
  {
    base::test::TestFuture<bool> future;
    IsUVPlatformAuthenticatorAvailable(/*browser_context=*/nullptr,
                                       future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  fake_api.set_available(true);
  fake_api.set_is_uvpaa(false);
  {
    base::test::TestFuture<bool> future;
    IsUVPlatformAuthenticatorAvailable(/*browser_context=*/nullptr,
                                       future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  fake_api.set_available(true);
  fake_api.set_is_uvpaa(true);
  {
    base::test::TestFuture<bool> future;
    IsUVPlatformAuthenticatorAvailable(/*browser_context=*/nullptr,
                                       future.GetCallback());
    EXPECT_TRUE(future.Get());
  }
}

#elif BUILDFLAG(IS_CHROMEOS)

TEST(IsUVPAATest, ChromeOS) {
  base::test::TaskEnvironment task_environment;
  chromeos::TpmManagerClient::InitializeFake();
  chromeos::U2FClient::InitializeFake();

  base::test::TestFuture<bool> future;
  IsUVPlatformAuthenticatorAvailable(/*browser_context=*/nullptr,
                                     future.GetCallback());
  EXPECT_FALSE(future.Get());

  chromeos::U2FClient::Shutdown();
  chromeos::TpmManagerClient::Shutdown();
}

#else

TEST(IsUVPAATest, UnsupportedPlatformReturnsFalse) {
  base::test::TaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  IsUVPlatformAuthenticatorAvailable(/*browser_context=*/nullptr,
                                     future.GetCallback());
  EXPECT_FALSE(future.Get());
}

#endif

}  // namespace
}  // namespace content
