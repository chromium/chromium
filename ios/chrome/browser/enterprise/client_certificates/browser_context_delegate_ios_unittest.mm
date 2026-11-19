// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/browser_context_delegate_ios.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/memory/scoped_refptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/features.h"
#import "components/enterprise/client_certificates/core/metrics_util.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "components/policy/core/common/cloud/cloud_policy_util.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/prefs/testing_pref_service.h"
#import "net/cert/x509_certificate.h"
#import "net/test/cert_builder.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace client_certificates {

namespace {

constexpr char kTestClientId[] = "test-ios-client-id";

void SetManagedAppConfiguration(NSDictionary* app_config) {
  [[NSUserDefaults standardUserDefaults]
      setObject:app_config
         forKey:kPolicyLoaderIOSConfigurationKey];
}

void ClearManagedAppConfiguration() {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

scoped_refptr<net::X509Certificate> CreateCert(const std::string& common_name,
                                               base::Time not_before,
                                               base::Time not_after) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  leaf->SetSubjectCommonName(common_name);
  leaf->SetValidity(not_before, not_after);
  return leaf->GetX509Certificate();
}

}  // namespace

class BrowserContextDelegateIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearManagedAppConfiguration();
    RegisterLocalStatePrefs(local_state_.registry());
    fake_dm_token_storage_.SetClientId(kTestClientId);
  }

  void TearDown() override {
    // `NSUserDefaults` persists across tests in the same process, so clean up.
    ClearManagedAppConfiguration();
    PlatformTest::TearDown();
  }

  BrowserContextDelegateIOS CreateDelegate() {
    return BrowserContextDelegateIOS(&local_state_);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kClientCertCommonNameAttribute};
  TestingPrefServiceSimple local_state_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
};

// Tests that `GetExpectedCommonName()` returns the browser client ID when the
// `ManagedClientCertificateForBrowserConfig` policy is unset.
TEST_F(BrowserContextDelegateIOSTest, GetExpectedCommonName_DefaultUnset) {
  EXPECT_EQ(kTestClientId, CreateDelegate().GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` returns the browser client ID when
// `common_name_source` is explicitly set to `device_id`.
TEST_F(BrowserContextDelegateIOSTest, GetExpectedCommonName_DefaultIdentifier) {
  local_state_.SetDict(
      prefs::kManagedClientCertificateForBrowserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey, kCommonNameSourceDeviceId));
  EXPECT_EQ(kTestClientId, CreateDelegate().GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` returns the sanitized computer name when
// `common_name_source` is set to `computer_name`.
TEST_F(BrowserContextDelegateIOSTest, GetExpectedCommonName_ComputerName) {
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  BrowserContextDelegateIOS delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("  Work iPad 01  ");
  EXPECT_EQ("Work iPad 01", delegate.GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` falls back to the browser client ID when
// `common_name_source` is set to `computer_name` and the sanitized name is
// empty.
TEST_F(BrowserContextDelegateIOSTest,
       GetExpectedCommonName_ComputerNameEmptyFallbackToClientId) {
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  BrowserContextDelegateIOS delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("   \t\r\n   ");
  EXPECT_EQ(kTestClientId, delegate.GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` reads and sanitizes the MDM `DeviceName`
// from Managed App Configuration when `common_name_source` is set to
// `computer_name`.
TEST_F(BrowserContextDelegateIOSTest,
       GetExpectedCommonName_ManagedAppConfigDeviceName) {
  SetManagedAppConfiguration(
      @{kPolicyLoaderIOSDeviceNameKey : @"  Work iPad 01  "});
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  EXPECT_EQ("Work iPad 01", CreateDelegate().GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` falls back to the browser client ID when
// `common_name_source` is set to `computer_name` and no MDM `DeviceName` is
// set.
TEST_F(BrowserContextDelegateIOSTest,
       GetExpectedCommonName_NoManagedAppConfigDeviceNameFallsBackToClientId) {
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  EXPECT_EQ(kTestClientId, CreateDelegate().GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` still uses the MDM `DeviceName` when
// `kIOSReportMachineName` is disabled.
TEST_F(
    BrowserContextDelegateIOSTest,
    GetExpectedCommonName_ManagedAppConfigDeviceNameIgnoresReportMachineNameFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(policy::kIOSReportMachineName);

  SetManagedAppConfiguration(
      @{kPolicyLoaderIOSDeviceNameKey : @"  Work iPad 01  "});
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  EXPECT_EQ("Work iPad 01", CreateDelegate().GetExpectedCommonName());
}

// Tests that `GetExpectedCommonName()` ignores `computer_name` and returns the
// browser client ID when `kClientCertCommonNameAttribute` is disabled.
TEST_F(BrowserContextDelegateIOSTest,
       GetExpectedCommonName_FeatureDisabledIgnoresComputerNamePolicy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kClientCertCommonNameAttribute);

  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  BrowserContextDelegateIOS delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("custom-ios-device");
  EXPECT_EQ(kTestClientId, delegate.GetExpectedCommonName());
}

// Tests that `ShouldCertificateBeRenewed()` returns false and logs `kValid`
// when a non-expiring certificate matches the expected browser client ID.
TEST_F(BrowserContextDelegateIOSTest,
       ShouldCertificateBeRenewed_MatchingClientId) {
  base::HistogramTester histogram_tester;
  BrowserContextDelegateIOS delegate = CreateDelegate();
  scoped_refptr<net::X509Certificate> cert =
      CreateCert(kTestClientId, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Browser.CertificateValidation.Result",
      CertificateValidationResult::kValid, 1);
}

// Tests that `ShouldCertificateBeRenewed()` returns false and logs `kValid`
// when `common_name_source` is `computer_name` and the certificate matches the
// device name.
TEST_F(BrowserContextDelegateIOSTest,
       ShouldCertificateBeRenewed_MatchingDeviceName) {
  constexpr char kTestDeviceName[] = "custom-ios-device";
  local_state_.SetDict(prefs::kManagedClientCertificateForBrowserConfigPrefs,
                       base::DictValue().Set(kCommonNameSourceKey,
                                             kCommonNameSourceComputerName));
  BrowserContextDelegateIOS delegate = CreateDelegate();
  delegate.SetComputerNameForTesting(kTestDeviceName);

  base::HistogramTester histogram_tester;
  scoped_refptr<net::X509Certificate> cert =
      CreateCert(kTestDeviceName, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Browser.CertificateValidation.Result",
      CertificateValidationResult::kValid, 1);
}

// Tests that `ShouldCertificateBeRenewed()` returns true and logs
// `kIdentityMismatch` when the certificate holds a device name but the policy
// requires `device_id`.
TEST_F(
    BrowserContextDelegateIOSTest,
    ShouldCertificateBeRenewed_DeviceNameRejectedWhenPolicyIsDefaultIdentifier) {
  constexpr char kTestDeviceName[] = "custom-ios-device";
  local_state_.SetDict(
      prefs::kManagedClientCertificateForBrowserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey, kCommonNameSourceDeviceId));
  BrowserContextDelegateIOS delegate = CreateDelegate();
  delegate.SetComputerNameForTesting(kTestDeviceName);

  base::HistogramTester histogram_tester;
  scoped_refptr<net::X509Certificate> cert =
      CreateCert(kTestDeviceName, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(30));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Browser.CertificateValidation.Result",
      CertificateValidationResult::kIdentityMismatch, 1);
}

// Tests that `GetIdentityName()` returns the managed browser identity constant.
TEST_F(BrowserContextDelegateIOSTest, GetIdentityName) {
  EXPECT_EQ(kManagedBrowserIdentityName, CreateDelegate().GetIdentityName());
}

// Tests that `GetTemporaryIdentityName()` returns the temporary managed browser
// identity constant.
TEST_F(BrowserContextDelegateIOSTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedBrowserIdentityName,
            CreateDelegate().GetTemporaryIdentityName());
}

// Tests that `GetPolicyPref()` returns the browser provisioning policy pref
// name.
TEST_F(BrowserContextDelegateIOSTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForBrowserPrefs,
            CreateDelegate().GetPolicyPref());
}

// Tests that `GetConfigPolicyPref()` returns the browser configuration policy
// pref name.
TEST_F(BrowserContextDelegateIOSTest, GetConfigPolicyPref) {
  EXPECT_EQ(prefs::kManagedClientCertificateForBrowserConfigPrefs,
            CreateDelegate().GetConfigPolicyPref());
}

// Tests that `GetLoggingContext()` returns the "Browser" logging context label.
TEST_F(BrowserContextDelegateIOSTest, GetLoggingContext) {
  EXPECT_EQ("Browser", CreateDelegate().GetLoggingContext());
}

}  // namespace client_certificates
