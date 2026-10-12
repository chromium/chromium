// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/public/features.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
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

std::unique_ptr<KeyedService> BuildSegmentationPlatformServiceWithLabels(
    std::vector<std::string> labels,
    ProfileIOS* profile) {
  if (!labels.empty()) {
    base::ListValue labels_list;
    for (const std::string& label : labels) {
      labels_list.Append(label);
    }
    profile->GetPrefs()->SetDict(
        kDeviceSwitcherUserSegmentPrefKey,
        base::DictValue().Set(
            "result", base::DictValue().Set("labels", std::move(labels_list))));
  }
  return segmentation_platform::SegmentationPlatformServiceFactory::
      GetDefaultFactory()
          .Run(profile);
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
        base::BindOnce(&BuildSegmentationPlatformServiceWithLabels,
                       std::move(device_switcher_labels)));
    return profile_manager_.AddProfileWithBuilder(std::move(builder));
  }

  ProfileIOS* AddSignedInProfile() {
    ProfileIOS* profile = AddProfileWithDeviceSwitcherLabels({});
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager())
        ->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile)->SignIn(
        identity, signin_metrics::AccessPoint::kStartPage);
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

// Tests that when the user is classified as an Android switcher, the
// .SegmentationPlatform.AndroidSwitcher variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsAndroidSwitcherVariant) {
  AddProfileWithDeviceSwitcherLabels(
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

// Tests that when the user has both Android phone and iOS phone Chrome labels,
// the .SegmentationPlatform.NotAndroidSwitcher variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsNotAndroidSwitcherVariant) {
  AddProfileWithDeviceSwitcherLabels(
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

// Tests that when the classification result is not ready, the
// .SegmentationPlatform.Unknown variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsUnknownVariantWhenNotReady) {
  AddProfileWithDeviceSwitcherLabels({});

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SegmentationPlatform.NotAndroidSwitcher"}),
        0);
  }
}

// Tests that when the device switcher classification times out waiting for
// sync device info (resulting in kNotSyncedLabel), the
// .SegmentationPlatform.Unknown variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsUnknownVariantOnTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          segmentation_platform::features::kSegmentationPlatformDeviceSwitcher,
          segmentation_platform::features::kSegmentationPlatformUmaFromSqlDb});
  AddProfileWithDeviceSwitcherLabels(
      {segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel});
  task_environment_.FastForwardBy(base::Seconds(60));

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SegmentationPlatform.Unknown"}), false, 1);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
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

// Tests that when the signed-in user's switcher info returns kUnknown, the
// .SwitcherInfo.Unknown variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoUnknownVariantWhenUnknownStatus) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
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
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when fetching switcher info fails (std::nullopt), the
// .SwitcherInfo.Unknown variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoUnknownVariantWhenFailed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
  ios::provider::test::SetSwitcherInfoResult(std::nullopt);
  AddSignedInProfile();

  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  task_environment_.FastForwardBy(base::Seconds(1));
  for (const char* histogram : kDefaultBrowserHistograms) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({histogram, ".SwitcherInfo.Unknown"}), false, 1);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when no signed-in profile exists, the .SwitcherInfo.Unknown
// variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       RecordsSwitcherInfoUnknownVariantWhenNotSignedIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
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
        base::StrCat({histogram, ".SwitcherInfo.AndroidSwitcher"}), 0);
    histogram_tester_.ExpectTotalCount(
        base::StrCat({histogram, ".SwitcherInfo.NotAndroidSwitcher"}), 0);
  }
}

// Tests that when kQuerySwitcherInfoSignalForDefaultBrowserMetrics is disabled,
// no .SwitcherInfo.* variants are recorded.
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest,
       DoesNotRecordSwitcherInfoVariantWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kQuerySwitcherInfoSignalForDefaultBrowserMetrics);
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
  }
}
