// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"

#import <memory>
#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/favicon_base/favicon_types.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/image_fetcher/core/mock_image_fetcher.h"
#import "components/image_fetcher/core/request_metadata.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios_factory.h"
#import "ios/chrome/browser/aim/model/ios_chrome_ai_mode_button_service_factory.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/mock_favicon_loader.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_unittest_util.h"

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr std::string_view kEntrypointShownHistogram =
    "Omnibox.AimEntrypoint.Shown";
constexpr std::string_view kEntrypointShownGoogleHistogram =
    "Omnibox.AimEntrypoint.Shown.google";
constexpr std::string_view kEntrypointShown3pHistogram =
    "Omnibox.AimEntrypoint.Shown.3p";
constexpr std::string_view kIconSourceHistogram =
    "Omnibox.AiModePageAction.IconSource";

class MockImageFetcherService : public image_fetcher::ImageFetcherService {
 public:
  MockImageFetcherService() {
    ON_CALL(*this, GetImageFetcher(_))
        .WillByDefault(Return(&mock_image_fetcher_));
  }
  MOCK_METHOD(image_fetcher::ImageFetcher*,
              GetImageFetcher,
              (image_fetcher::ImageFetcherConfig),
              (override));

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return &mock_image_fetcher_;
  }

 private:
  NiceMock<image_fetcher::MockImageFetcher> mock_image_fetcher_;
};

std::unique_ptr<KeyedService> CreateMockAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

std::unique_ptr<KeyedService> CreateMockFaviconLoader(ProfileIOS* profile) {
  return std::make_unique<NiceMock<MockFaviconLoader>>();
}

std::unique_ptr<KeyedService> CreateMockFaviconService(ProfileIOS* profile) {
  return std::make_unique<NiceMock<favicon::MockFaviconService>>();
}

std::unique_ptr<KeyedService> CreateMockImageFetcherService(
    ProfileIOS* profile) {
  return std::make_unique<NiceMock<MockImageFetcherService>>();
}

class AIModeButtonServiceIOSTest : public PlatformTest {
 protected:
  AIModeButtonServiceIOSTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockAimEligibilityService));
    builder.AddTestingFactory(IOSChromeFaviconLoaderFactory::GetInstance(),
                              base::BindRepeating(&CreateMockFaviconLoader));
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockFaviconService));
    builder.AddTestingFactory(
        ImageFetcherServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockImageFetcherService));
    profile_ = std::move(builder).Build();

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();

    // Default factory sets Google as DSE.
    google_turl_ = template_url_service_->GetDefaultSearchProvider();

    // Add Bing search engine (used by 3P debug config).
    TemplateURLData bing_data;
    bing_data.SetShortName(u"Bing");
    bing_data.SetKeyword(u"bing");
    bing_data.SetURL("https://www.bing.com/search?q={searchTerms}");
    bing_turl_ =
        template_url_service_->Add(std::make_unique<TemplateURL>(bing_data));

    // Add a 3P engine without AIM config.
    TemplateURLData nongoogle_data;
    nongoogle_data.SetShortName(u"NonGoogle");
    nongoogle_data.SetKeyword(u"nongoogle");
    nongoogle_data.SetURL("https://www.nongoogle.com/search?q={searchTerms}");
    nongoogle_turl_ = template_url_service_->Add(
        std::make_unique<TemplateURL>(nongoogle_data));

    aim_eligibility_service_ = static_cast<MockIOSChromeAimEligibilityService*>(
        IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get()));
    favicon_loader_ = static_cast<MockFaviconLoader*>(
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get()));
    favicon_service_ = static_cast<favicon::MockFaviconService*>(
        ios::FaviconServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
    image_fetcher_service_ = static_cast<MockImageFetcherService*>(
        ImageFetcherServiceFactory::GetForProfile(profile_.get()));

    service_ = AIModeButtonServiceIOSFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<MockIOSChromeAimEligibilityService> aim_eligibility_service_ =
      nullptr;
  raw_ptr<MockFaviconLoader> favicon_loader_ = nullptr;
  raw_ptr<favicon::MockFaviconService> favicon_service_ = nullptr;
  raw_ptr<MockImageFetcherService> image_fetcher_service_ = nullptr;
  raw_ptr<AIModeButtonServiceIOS> service_ = nullptr;

  raw_ptr<const TemplateURL> google_turl_ = nullptr;
  raw_ptr<TemplateURL> bing_turl_ = nullptr;
  raw_ptr<TemplateURL> nongoogle_turl_ = nullptr;
};

// Tests Google DSE button availability and properties.
TEST_F(AIModeButtonServiceIOSTest, GoogleDseProperties) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->IsButtonAvailable());
  service_->RecordEntrypointShown(true);
  histogram_tester.ExpectBucketCount(kEntrypointShownHistogram, true, 1);
  histogram_tester.ExpectBucketCount(kEntrypointShownGoogleHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kEntrypointShown3pHistogram, 0);
  EXPECT_NSEQ(service_->GetTitle(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
  EXPECT_NSEQ(service_->GetAccessibilityLabel(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
  EXPECT_NE(service_->GetIcon(), nil);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram, AIModeButtonServiceIOS::IconSource::kVectorIcon, 1);
  EXPECT_TRUE(service_->GetUrl().is_valid());

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(service_->IsButtonAvailable());
  service_->RecordEntrypointShown(false);
  histogram_tester.ExpectBucketCount(kEntrypointShownHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kEntrypointShownGoogleHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kEntrypointShown3pHistogram, 0);
  EXPECT_NE(service_->GetIcon(), nil);
  histogram_tester.ExpectBucketCount(
      kIconSourceHistogram, AIModeButtonServiceIOS::IconSource::kInvisible, 1);
}

// Tests 3P DSE when the 3P entrypoint feature is disabled.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kAim3pEntrypoint);

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(service_->IsButtonAvailable());
}

// Tests 3P DSE button availability and properties when enabled.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseEnabledWithDebugConfig) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->IsButtonAvailable());
  service_->RecordEntrypointShown(true);
  histogram_tester.ExpectBucketCount(kEntrypointShownHistogram, true, 1);
  histogram_tester.ExpectBucketCount(kEntrypointShown3pHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kEntrypointShownGoogleHistogram, 0);
  EXPECT_NSEQ(service_->GetTitle(), @"AI Mode for Bing (ĄÜÔ)");
  // `AiModeButtonService` populates `a11y_label` using
  // `IDS_AI_MODE_ENTRYPOINT_ACC_LABEL` formatted with the button text.
  EXPECT_NSEQ(
      service_->GetAccessibilityLabel(),
      base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_AI_MODE_ENTRYPOINT_ACC_LABEL, u"AI Mode for Bing (ĄÜÔ)")));
  EXPECT_NE(service_->GetIcon(), nil);
  EXPECT_TRUE(service_->GetUrl().is_valid());

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(service_->IsButtonAvailable());
  service_->RecordEntrypointShown(false);
  histogram_tester.ExpectBucketCount(kEntrypointShownHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kEntrypointShown3pHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kEntrypointShownGoogleHistogram, 0);
}

// Tests 3P DSE without AIM config.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseWithoutAimConfig) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});

  template_url_service_->SetUserSelectedDefaultSearchProvider(nongoogle_turl_);

  EXPECT_FALSE(service_->IsButtonAvailable());
  service_->RecordEntrypointShown(false);
  histogram_tester.ExpectUniqueSample(kEntrypointShownHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kEntrypointShownGoogleHistogram, 0);
  histogram_tester.ExpectTotalCount(kEntrypointShown3pHistogram, 0);
  EXPECT_NSEQ(service_->GetTitle(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
  EXPECT_NSEQ(service_->GetAccessibilityLabel(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
}

// Tests state change notifications when DSE changes.
TEST_F(AIModeButtonServiceIOSTest, StateChangedNotificationOnDseChange) {
  base::MockRepeatingClosure state_changed_callback;
  base::CallbackListSubscription subscription =
      service_->RegisterStateChangedCallback(state_changed_callback.Get());

  EXPECT_CALL(state_changed_callback, Run()).Times(testing::AtLeast(1));
  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
}

// Tests 3P favicon loading from FaviconLoader disk/DB cache and subsequent
// memory cache hits.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyFaviconFromFaviconLoader) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});
  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));

  UIImage* test_image = gfx::test::CreateImage(18, 18).ToUIImage();
  EXPECT_CALL(*favicon_loader_, FaviconForIconUrl(_, _, _, _))
      .WillOnce([test_image](
                    const GURL&, float, float,
                    FaviconLoader::FaviconAttributesCompletionBlock callback) {
        // First synchronous monogram placeholder should be ignored.
        callback([FaviconAttributes attributesWithMonogram:@"B"
                                                 textColor:UIColor.clearColor
                                           backgroundColor:UIColor.clearColor
                                    defaultBackgroundColor:YES],
                 /*cached=*/true);
        // Async database result provides the image.
        callback([FaviconAttributes attributesWithImage:test_image],
                 /*cached=*/false);
      });
  EXPECT_CALL(*image_fetcher_service_->mock_image_fetcher(),
              FetchImageAndData_(_, _, _, _))
      .Times(0);

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram,
      AIModeButtonServiceIOS::IconSource::kDiskDbFaviconCache, 1);

  UIImage* icon = service_->GetIcon();
  ASSERT_NE(icon, nil);
  EXPECT_EQ(icon.renderingMode, UIImageRenderingModeAlwaysOriginal);
  histogram_tester.ExpectBucketCount(
      kIconSourceHistogram,
      AIModeButtonServiceIOS::IconSource::kMemoryFaviconCache, 1);
}

// Tests 3P favicon loading from FaviconLoader synchronous memory cache.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyFaviconFromMemoryCache) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});
  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));

  UIImage* test_image = gfx::test::CreateImage(18, 18).ToUIImage();
  EXPECT_CALL(*favicon_loader_, FaviconForIconUrl(_, _, _, _))
      .WillOnce([test_image](
                    const GURL&, float, float,
                    FaviconLoader::FaviconAttributesCompletionBlock callback) {
        callback([FaviconAttributes attributesWithImage:test_image],
                 /*cached=*/true);
      });

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram,
      AIModeButtonServiceIOS::IconSource::kMemoryFaviconCache, 1);

  UIImage* icon = service_->GetIcon();
  ASSERT_NE(icon, nil);
  EXPECT_EQ(icon.renderingMode, UIImageRenderingModeAlwaysOriginal);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram,
      AIModeButtonServiceIOS::IconSource::kMemoryFaviconCache, 2);
}

// Tests 3P favicon network fetch fallback and persistence to FaviconService.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyFaviconFromNetworkFallback) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});
  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*favicon_loader_, FaviconForIconUrl(_, _, _, _))
      .WillOnce([](const GURL&, float, float,
                   FaviconLoader::FaviconAttributesCompletionBlock callback) {
        callback([FaviconAttributes attributesWithMonogram:@"B"
                                                 textColor:UIColor.clearColor
                                           backgroundColor:UIColor.clearColor
                                    defaultBackgroundColor:YES],
                 /*cached=*/false);
      });

  gfx::Image fetched_image = gfx::test::CreateImage(32, 32);
  EXPECT_CALL(*image_fetcher_service_->mock_image_fetcher(),
              FetchImageAndData_(_, _, _, _))
      .WillOnce([fetched_image](const GURL&,
                                image_fetcher::ImageDataFetcherCallback*,
                                image_fetcher::ImageFetcherCallback* callback,
                                image_fetcher::ImageFetcherParams) {
        std::move(*callback).Run(fetched_image,
                                 image_fetcher::RequestMetadata());
      });
  EXPECT_CALL(*favicon_service_,
              SetFavicons(_, _, favicon_base::IconType::kFavicon, _))
      .Times(1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram, AIModeButtonServiceIOS::IconSource::kNetworkFetch,
      1);

  UIImage* icon = service_->GetIcon();
  ASSERT_NE(icon, nil);
  EXPECT_EQ(icon.renderingMode, UIImageRenderingModeAlwaysOriginal);
  histogram_tester.ExpectBucketCount(
      kIconSourceHistogram,
      AIModeButtonServiceIOS::IconSource::kMemoryFaviconCache, 1);
}

// Tests 3P favicon network failure falls back to monochrome search symbol.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyFaviconNetworkFailureFallback) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});
  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*favicon_loader_, FaviconForIconUrl(_, _, _, _))
      .WillOnce([](const GURL&, float, float,
                   FaviconLoader::FaviconAttributesCompletionBlock callback) {
        callback([FaviconAttributes attributesWithMonogram:@"B"
                                                 textColor:UIColor.clearColor
                                           backgroundColor:UIColor.clearColor
                                    defaultBackgroundColor:YES],
                 /*cached=*/false);
      });
  EXPECT_CALL(*image_fetcher_service_->mock_image_fetcher(),
              FetchImageAndData_(_, _, _, _))
      .WillOnce([](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                   image_fetcher::ImageFetcherCallback* callback,
                   image_fetcher::ImageFetcherParams) {
        std::move(*callback).Run(gfx::Image(),
                                 image_fetcher::RequestMetadata());
      });
  EXPECT_CALL(*favicon_service_, SetFavicons(_, _, _, _)).Times(0);

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
  histogram_tester.ExpectUniqueSample(
      kIconSourceHistogram, AIModeButtonServiceIOS::IconSource::kFailedIcon, 1);

  UIImage* icon = service_->GetIcon();
  ASSERT_NE(icon, nil);
  EXPECT_NE(icon.renderingMode, UIImageRenderingModeAlwaysOriginal);
}

// Tests that 3P favicon is loaded on service construction when a 3P DSE is
// already selected.
TEST_F(AIModeButtonServiceIOSTest,
       ThirdPartyFaviconLoadedOnServiceConstruction) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});
  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(Return(true));

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);

  UIImage* test_image = gfx::test::CreateImage(18, 18).ToUIImage();
  EXPECT_CALL(*favicon_loader_, FaviconForIconUrl(_, _, _, _))
      .WillOnce([test_image](
                    const GURL&, float, float,
                    FaviconLoader::FaviconAttributesCompletionBlock callback) {
        callback([FaviconAttributes attributesWithImage:test_image],
                 /*cached=*/true);
      });

  AIModeButtonServiceIOS constructed_service(
      template_url_service_, aim_eligibility_service_,
      IOSChromeAiModeButtonServiceFactory::GetForProfile(profile_.get()),
      favicon_loader_, favicon_service_, image_fetcher_service_);

  UIImage* icon = constructed_service.GetIcon();
  ASSERT_NE(icon, nil);
  EXPECT_EQ(icon.renderingMode, UIImageRenderingModeAlwaysOriginal);
  constructed_service.Shutdown();
}

}  // namespace
