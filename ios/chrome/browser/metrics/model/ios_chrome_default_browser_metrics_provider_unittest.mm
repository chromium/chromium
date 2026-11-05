// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/metrics/field_trial.h"
#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/public/features.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/switcher_info/test_switcher_info.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

constexpr const char* kDefaultBrowserHistograms[] = {
    "IOS.IsDefaultBrowser",   "IOS.IsDefaultBrowser1",
    "IOS.IsDefaultBrowser3",  "IOS.IsDefaultBrowser14",
    "IOS.IsDefaultBrowser21", "IOS.IsDefaultBrowser28",
    "IOS.IsDefaultBrowser35", "IOS.IsDefaultBrowser42",
};

constexpr char kDeviceSwitcherUserSegmentPrefKey[] =
    "segmentation_platform.device_switcher_util";

ProfileIOS& SetupProfileForSegmentationPlatformService(
    std::vector<std::string> labels,
    ProfileIOS& profile) {
  if (!labels.empty()) {
    base::ListValue labels_list;
    for (const std::string& label : labels) {
      labels_list.Append(label);
    }
    profile.GetPrefs()->SetDict(
        kDeviceSwitcherUserSegmentPrefKey,
        base::DictValue().Set(
            "result", base::DictValue().Set("labels", std::move(labels_list))));
  }
  return profile;
}

}  // namespace

// Tests metrics that are recorded and uploaded by
// IOSChromeDefaultBrowserMetricsProvider.
class IOSChromeDefaultBrowserMetricsProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();
  }

  void TearDown() override {
    ios::provider::test::ResetSwitcherInfoState();
    PlatformTest::TearDown();
  }

  ProfileIOS* AddProfileWithDeviceSwitcherLabels(
      std::vector<std::string> device_switcher_labels) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        base::BindOnce(&SetupProfileForSegmentationPlatformService,
                       std::move(device_switcher_labels))
            .Then(segmentation_platform::SegmentationPlatformServiceFactory::
                      GetDefaultFactory()));
    return profile_manager_.AddProfileWithBuilder(std::move(builder));
  }

  ProfileIOS* AddSignedInProfile(
      const std::vector<id<SystemIdentity>>& identities =
          {[FakeSystemIdentity fakeIdentity1]},
      std::vector<std::string> device_switcher_labels = {}) {
    CHECK(!identities.empty());
    ProfileIOS* profile =
        AddProfileWithDeviceSwitcherLabels(std::move(device_switcher_labels));
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    for (id<SystemIdentity> identity : identities) {
      system_identity_manager->AddIdentity(identity);
    }
    AuthenticationServiceFactory::GetForProfile(profile)->SignIn(
        identities.front(), signin_metrics::AccessPoint::kStartPage);
    return profile;
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::HistogramTester histogram_tester_;
};

// Tests the implementation of OnDidCreateMetricsLog() without any loaded
// profiles (records SegmentationPlatform.Unknown signal status).
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest, OnDidCreateMetricsLog) {
  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(histogram, false, 1);
    histogram_tester_.ExpectBucketCount(histogram, true, 0);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), true, 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(histogram, true, 1);
    histogram_tester_.ExpectBucketCount(histogram, false, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
  }
}

// Tests that when the syncing primary account is classified as an Android
// switcher, the `.SegmentationPlatform.AndroidSwitcher` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsAndroidSwitcherVariant) {
  AddSignedInProfile(
      {[FakeSystemIdentity fakeIdentity1]},
      {segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}),
        false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}),
        true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}),
        false, 1);
  }
}

// Tests that when the syncing primary account has both Android phone and iOS
// phone Chrome labels, the `.SegmentationPlatform.NotAndroidSwitcher` variants
// are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsNotAndroidSwitcherVariant) {
  AddSignedInProfile(
      {[FakeSystemIdentity fakeIdentity1]},
      {segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
       segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        false, 1);
  }
}

// Tests that when the forced device switcher flag is set to `AndroidPhone`,
// both `.SegmentationPlatform.AndroidSwitcher` and
// `.SwitcherInfo.AndroidSwitcher` variants are recorded even if the provider
// fails (`std::nullopt`).
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsAndroidSwitcherVariantsWhenForcedDeviceSwitcherFlagSet) {
  ios::provider::test::SetSwitcherInfoResult(std::nullopt);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "force-device-switcher-experience",
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel);

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}),
        false, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
  }
}

// Tests that when the forced device switcher flag is set to a non-Android
// segment, both `.SegmentationPlatform.NotAndroidSwitcher` and
// `.SwitcherInfo.NotAndroidSwitcher` variants are recorded even if the
// provider returns `kSwitcher`.
TEST_F(
    IOSChromeDefaultBrowserMetricsProviderTest,
    RecordsNotAndroidSwitcherVariantsWhenForcedNonAndroidDeviceSwitcherFlagSet) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "force-device-switcher-experience",
      segmentation_platform::DeviceSwitcherModel::kDesktopLabel);

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        false, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), false,
        1);
  }
}

// Tests that when a syncing primary account exists and the classification
// result is not ready, the `.SegmentationPlatform.ServiceError` variants are
// recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsServiceErrorVariantWhenNotReady) {
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), false,
        1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
  }
}

// Tests that when a syncing primary account exists and the device switcher
// classification times out waiting for sync device info (resulting in
// `kNotSyncedLabel`), the `.SegmentationPlatform.ServiceError` variants are
// recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsServiceErrorVariantOnTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          segmentation_platform::features::kSegmentationPlatformDeviceSwitcher,
          segmentation_platform::features::kSegmentationPlatformUmaFromSqlDb});
  AddSignedInProfile(
      {[FakeSystemIdentity fakeIdentity1]},
      {segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel});
  task_environment_.FastForwardBy(base::Seconds(60));

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), false,
        1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
  }
}

// Tests that when an on-device identity exists in the loaded profile but the
// user is not signed in to Chrome, the `.SegmentationPlatform.Unknown` variants
// are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsUnknownVariantWhenNotSignedIn) {
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  AddProfileWithDeviceSwitcherLabels(
      {segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
  }
}

// Tests that when a primary account is signed in but not syncing, the
// `.SegmentationPlatform.Unknown` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsUnknownVariantWhenPrimaryAccountNotSyncing) {
  ProfileIOS* profile = AddSignedInProfile(
      {[FakeSystemIdentity fakeIdentity1]},
      {segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});
  static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile))
      ->SetPersistentAuthError();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.ServiceError"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
  }
}

// Tests that when the signed-in user's switcher info returns kSwitcher, the
// .SwitcherInfo.AndroidSwitcher variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoAndroidSwitcherVariant) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
  }
}

// Tests that when the signed-in user's switcher info returns kNotSwitcher, the
// .SwitcherInfo.NotAndroidSwitcher variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoNotAndroidSwitcherVariant) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kNotSwitcher,
      .is_chrome_user = false});
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), false,
        1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), false,
        1);
  }
}

// Tests that when the signed-in user's switcher info returns `kUnknown`, the
// `.SwitcherInfo.ServiceError` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoServiceErrorVariantWhenUnknownStatus) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kUnknown,
      .is_chrome_user = false});
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when fetching switcher info fails (`std::nullopt`), the
// `.SwitcherInfo.ServiceError` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoServiceErrorVariantWhenFailed) {
  ios::provider::test::SetSwitcherInfoResult(std::nullopt);
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when an on-device consumer identity exists in the personal profile
// even when not signed into Chrome, `.SwitcherInfo.AndroidSwitcher` is recorded
// when `GetSwitcherInfo` returns `kSwitcher`.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoAndroidSwitcherVariantWhenNotSignedIn) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  AddProfileWithDeviceSwitcherLabels({});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }
}

// Tests that when there are no on-device identities in the personal profile,
// the `.SwitcherInfo.Unknown` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoUnknownVariantWhenNoIdentitiesOnDevice) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  AddProfileWithDeviceSwitcherLabels({});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when only a managed identity exists on the device, the consumer
// profile has no identities and the `.SwitcherInfo.Unknown` variants are
// recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoUnknownVariantWhenOnlyManagedIdentityOnDevice) {
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  AddProfileWithDeviceSwitcherLabels({});
  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(managed_identity);

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when a secondary identity in a signed-in profile has kSwitcher
// status while the primary identity has kNotSwitcher, all signed-in identities
// are queried and .SwitcherInfo.AndroidSwitcher is recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoAndroidSwitcherFromSecondaryIdentity) {
  FakeSystemIdentity* primary_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* secondary_identity = [FakeSystemIdentity fakeIdentity2];

  ios::provider::test::SetSwitcherInfoResultForIdentity(
      primary_identity,
      ios::provider::SwitcherInfoResult{
          .switcher_status = ios::provider::SwitcherStatus::kNotSwitcher,
          .is_chrome_user = false});
  ios::provider::test::SetSwitcherInfoResultForIdentity(
      secondary_identity,
      ios::provider::SwitcherInfoResult{
          .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
          .is_chrome_user = true});

  AddSignedInProfile({primary_identity, secondary_identity});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), true, 1);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
  }
}

// Tests that when the primary identity has kUnknown status and a secondary
// identity in the signed-in profile has kNotSwitcher, all signed-in identities
// are queried and .SwitcherInfo.NotAndroidSwitcher is recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoNotAndroidSwitcherFromSecondaryIdentity) {
  FakeSystemIdentity* primary_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* secondary_identity = [FakeSystemIdentity fakeIdentity2];

  ios::provider::test::SetSwitcherInfoResultForIdentity(
      primary_identity,
      ios::provider::SwitcherInfoResult{
          .switcher_status = ios::provider::SwitcherStatus::kUnknown,
          .is_chrome_user = false});
  ios::provider::test::SetSwitcherInfoResultForIdentity(
      secondary_identity,
      ios::provider::SwitcherInfoResult{
          .switcher_status = ios::provider::SwitcherStatus::kNotSwitcher,
          .is_chrome_user = false});

  AddSignedInProfile({primary_identity, secondary_identity});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), false,
        1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }
}

// Tests that when `kRecordSwitcherInfoDefaultBrowserMetrics` is disabled, no
// `.SwitcherInfo.*` variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       DoesNotRecordSwitcherInfoVariantWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      first_run::kRecordSwitcherInfoDefaultBrowserMetrics);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.ServiceError"}), 0);
  }
}

// Tests that `.SwitcherInfo.*` variants are recorded without activating the
// `kAndroidSwitcherDefaultBrowserPromoInFRE` Finch trial.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoVariantWithoutActivatingPromoTrial) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
      {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
        first_run::
            kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal}});
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), false, 1);
  }
  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(
      first_run::kAndroidSwitcherDefaultBrowserPromoInFRE);
  ASSERT_TRUE(trial);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
}
