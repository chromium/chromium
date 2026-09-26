// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_ephemeral_theme_promo_mediator.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/themes/ntp_background_service.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake consumer for `HomeCustomizationEphemeralThemePromoMediator` tests.
@interface FakeHomeCustomizationEphemeralThemePromoConsumer
    : NSObject <HomeCustomizationEphemeralThemePromoConsumer>

@property(nonatomic, copy) NSString* animationAssetName;
@property(nonatomic, strong) NSBundle* bundle;
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* lightModeColorProvider;
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* darkModeColorProvider;
@property(nonatomic, assign) BOOL wasConfigured;

@end

@implementation FakeHomeCustomizationEphemeralThemePromoConsumer

- (void)setAnimationAssetName:(NSString*)animationAssetName
                       bundle:(NSBundle*)bundle {
  self.animationAssetName = animationAssetName;
  self.bundle = bundle;
  self.wasConfigured = YES;
}

- (void)setLightModeColorProvider:
            (NSDictionary<NSString*, UIColor*>*)lightModeColorProvider
            darkModeColorProvider:
                (NSDictionary<NSString*, UIColor*>*)darkModeColorProvider {
  self.lightModeColorProvider = lightModeColorProvider;
  self.darkModeColorProvider = darkModeColorProvider;
}

@end

// Unit tests for `HomeCustomizationEphemeralThemePromoMediator`.
class HomeCustomizationEphemeralThemePromoMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    HomeBackgroundCustomizationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kNTPCustomBackgroundEnabledByPolicy, true);
    pref_service_->registry()->RegisterIntegerPref(themes::kPolicyThemeColor,
                                                   SK_ColorTRANSPARENT);

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    application_locale_storage_ = std::make_unique<ApplicationLocaleStorage>();
    ntp_background_service_ = std::make_unique<NtpBackgroundService>(
        application_locale_storage_.get(), test_shared_loader_factory_);
    user_image_manager_ = std::make_unique<FakeUserUploadedImageManager>(
        base::SequencedTaskRunner::GetCurrentDefault());
    background_image_service_ =
        std::make_unique<FakeHomeBackgroundImageService>(
            ntp_background_service_.get());
    background_customization_service_ =
        std::make_unique<HomeBackgroundCustomizationService>(
            pref_service_.get(), user_image_manager_.get(),
            background_image_service_.get(), /*url_loader_factory=*/nullptr,
            base::FilePath());

    consumer_ = [[FakeHomeCustomizationEphemeralThemePromoConsumer alloc] init];
    mediator_ = [[HomeCustomizationEphemeralThemePromoMediator alloc]
                   initWithPrefService:pref_service_.get()
        backgroundCustomizationService:background_customization_service_.get()
                    promoDataDirectory:temp_dir_.GetPath()];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    consumer_ = nil;
    background_customization_service_->Shutdown();
    background_customization_service_.reset();
    ntp_background_service_->Shutdown();
    ntp_background_service_.reset();
    PlatformTest::TearDown();
  }

  // Creates the dummy Lottie animation JSON file in the expected promo bundle
  // directory inside `temp_dir_` and returns its file path.
  base::FilePath WriteAnimationFileToDisk() {
    base::FilePath bundle_dir =
        temp_dir_.GetPath().AppendASCII(kEphemeralThemeDirectoryName);
    EXPECT_TRUE(base::CreateDirectory(bundle_dir));
    base::FilePath json_path =
        bundle_dir.AppendASCII(kEphemeralThemePromoAnimationFileName);
    EXPECT_TRUE(base::WriteFile(json_path, R"({"v":"5.7.4"})"));
    return json_path;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  std::unique_ptr<NtpBackgroundService> ntp_background_service_;
  std::unique_ptr<FakeUserUploadedImageManager> user_image_manager_;
  std::unique_ptr<FakeHomeBackgroundImageService> background_image_service_;
  std::unique_ptr<HomeBackgroundCustomizationService>
      background_customization_service_;
  FakeHomeCustomizationEphemeralThemePromoConsumer* consumer_;
  HomeCustomizationEphemeralThemePromoMediator* mediator_;
};

// Test that the mediator parses both "#RRGGBB" and "RRGGBB" hex strings in
// light and dark mode color mappings and configures the consumer.
TEST_F(HomeCustomizationEphemeralThemePromoMediatorTest,
       ConfiguresConsumerWithValidHexColors) {
  base::FilePath promo_file_path = WriteAnimationFileToDisk();

  base::DictValue light_dict;
  light_dict.Set("**.Background.Fill 1.Color", "#1A73E8");
  light_dict.Set("**.Text.Fill 1.Color", "34A853");
  base::DictValue dark_dict;
  dark_dict.Set("**.Background.Fill 1.Color", "#8AB4F8");
  base::DictValue color_mapping_dict;
  color_mapping_dict.Set("light", std::move(light_dict));
  color_mapping_dict.Set("dark", std::move(dark_dict));

  base::DictValue theme_dict;
  theme_dict.Set(kEphemeralThemeAnimationPromoPathKey, promo_file_path.value());
  theme_dict.Set(kEphemeralThemeAnimationPromoColorMappingKey,
                 std::move(color_mapping_dict));
  pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                         std::move(theme_dict));

  mediator_.consumer = consumer_;

  EXPECT_TRUE(consumer_.wasConfigured);
  EXPECT_NSEQ(@"ephemeral_promo", consumer_.animationAssetName);
  EXPECT_NE(nil, consumer_.bundle);

  ASSERT_NE(nil, consumer_.lightModeColorProvider);
  EXPECT_EQ(2u, consumer_.lightModeColorProvider.count);
  EXPECT_NSEQ(UIColorFromRGB(0x1A73E8),
              consumer_.lightModeColorProvider[@"**.Background.Fill 1.Color"]);
  EXPECT_NSEQ(UIColorFromRGB(0x34A853),
              consumer_.lightModeColorProvider[@"**.Text.Fill 1.Color"]);

  ASSERT_NE(nil, consumer_.darkModeColorProvider);
  EXPECT_EQ(1u, consumer_.darkModeColorProvider.count);
  EXPECT_NSEQ(UIColorFromRGB(0x8AB4F8),
              consumer_.darkModeColorProvider[@"**.Background.Fill 1.Color"]);
}

// Test that malformed or invalid hex color entries are skipped while valid
// entries are still applied.
TEST_F(HomeCustomizationEphemeralThemePromoMediatorTest,
       SkipsInvalidHexColorsInColorMapping) {
  base::FilePath promo_file_path = WriteAnimationFileToDisk();

  base::DictValue light_dict;
  light_dict.Set("valid", "#FF5733");
  light_dict.Set("too_short", "#123");
  light_dict.Set("non_hex", "#GGGGGG");
  light_dict.Set("empty", "");
  base::DictValue dark_dict;
  dark_dict.Set("also_invalid", "#12345");
  base::DictValue color_mapping_dict;
  color_mapping_dict.Set("light", std::move(light_dict));
  color_mapping_dict.Set("dark", std::move(dark_dict));

  base::DictValue theme_dict;
  theme_dict.Set(kEphemeralThemeAnimationPromoPathKey, promo_file_path.value());
  theme_dict.Set(kEphemeralThemeAnimationPromoColorMappingKey,
                 std::move(color_mapping_dict));
  pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                         std::move(theme_dict));

  mediator_.consumer = consumer_;

  EXPECT_TRUE(consumer_.wasConfigured);
  ASSERT_NE(nil, consumer_.lightModeColorProvider);
  EXPECT_EQ(1u, consumer_.lightModeColorProvider.count);
  EXPECT_NSEQ(UIColorFromRGB(0xFF5733),
              consumer_.lightModeColorProvider[@"valid"]);
  EXPECT_EQ(nil, consumer_.darkModeColorProvider);
}

// Test that the consumer is not configured when the ephemeral theme pref is not
// set.
TEST_F(HomeCustomizationEphemeralThemePromoMediatorTest,
       DoesNotConfigureConsumerWhenPrefIsMissing) {
  WriteAnimationFileToDisk();

  mediator_.consumer = consumer_;

  EXPECT_FALSE(consumer_.wasConfigured);
  EXPECT_EQ(nil, consumer_.animationAssetName);
}

// Test that the consumer is not configured when the pref is set but the
// animation JSON file does not exist on disk.
TEST_F(HomeCustomizationEphemeralThemePromoMediatorTest,
       DoesNotConfigureConsumerWhenAnimationFileIsMissing) {
  base::FilePath missing_path =
      temp_dir_.GetPath()
          .AppendASCII(kEphemeralThemeDirectoryName)
          .AppendASCII(kEphemeralThemePromoAnimationFileName);
  base::DictValue theme_dict;
  theme_dict.Set(kEphemeralThemeAnimationPromoPathKey, missing_path.value());
  pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                         std::move(theme_dict));

  mediator_.consumer = consumer_;

  EXPECT_FALSE(consumer_.wasConfigured);
}

// Test that `applyEphemeralTheme` sets and stores the ephemeral theme on the
// background customization service using the saved seed color.
TEST_F(HomeCustomizationEphemeralThemePromoMediatorTest,
       AppliesAndStoresEphemeralTheme) {
  base::DictValue theme_dict;
  theme_dict.Set(kEphemeralThemeSeedColorKey, "#1A73E8");
  pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                         std::move(theme_dict));

  EXPECT_FALSE(background_customization_service_->IsCurrentEphemeralTheme());

  [mediator_ applyEphemeralTheme];

  EXPECT_TRUE(background_customization_service_->IsCurrentEphemeralTheme());
  std::optional<sync_pb::UserColorTheme> color_theme =
      background_customization_service_->GetCurrentColorTheme();
  ASSERT_TRUE(color_theme.has_value());
  EXPECT_EQ(SkColorSetA(0x1A73E8, 0xFF), color_theme->color());
  EXPECT_EQ(sync_pb::UserColorTheme::TONAL_SPOT,
            color_theme->browser_color_variant());
}
