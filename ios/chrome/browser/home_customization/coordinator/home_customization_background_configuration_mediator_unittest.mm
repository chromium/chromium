// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_configuration_mediator.h"

#import <variant>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/mock_image_fetcher.h"
#import "components/image_fetcher/core/request_metadata.h"
#import "components/prefs/pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"
#import "ios/chrome/browser/home_customization/model/home_customization_seed_colors.h"
#import "ios/chrome/browser/home_customization/model/ntp_background_service_factory.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image_unittest_util.h"

namespace {

std::unique_ptr<KeyedService> CreateBackgroundImageService(
    ProfileIOS* profile) {
  return std::make_unique<FakeHomeBackgroundImageService>(
      NtpBackgroundServiceFactory::GetForProfile(profile));
}

std::unique_ptr<KeyedService> CreateUserUploadedImageManager(
    ProfileIOS* profile) {
  return std::make_unique<FakeUserUploadedImageManager>(
      base::SequencedTaskRunner::GetCurrentDefault());
}

// Post reply to image fetch. `p0` represents the image to return. `p1`
// represents the HTTP response code.
ACTION_P(PostFetchReply, image, http_response_code) {
  image_fetcher::RequestMetadata metadata;
  metadata.http_response_code = http_response_code;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*arg2), image, metadata));
}

}  // namespace

// HomeCustomizationBackgroundConfigurationConsumer for use in tests.
@interface FakeHomeCustomizationBackgroundConfigurationConsumer
    : NSObject <HomeCustomizationBackgroundConfigurationConsumer>

@property(nonatomic, copy)
    NSArray<BackgroundCollectionConfiguration*>* configurations;

@property(nonatomic, copy) NSString* selectedBackgroundId;

@end

@implementation FakeHomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  self.configurations = backgroundCollectionConfigurations;
  self.selectedBackgroundId = selectedBackgroundId;
}

- (void)currentBackgroundConfigurationChanged:
    (id<BackgroundCustomizationConfiguration>)currentConfiguration {
  self.selectedBackgroundId = currentConfiguration.configurationID;
}

@end

// Tests for the Home Customization Background Configuration mediator.
class HomeCustomizationBackgroundConfigurationMediatorTest
    : public PlatformTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kNTPBackgroundCustomization);

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.SetPrefService(CreatePrefService());
    test_profile_builder.AddTestingFactory(
        UserUploadedImageManagerFactory::GetInstance(),
        base::BindRepeating(&CreateUserUploadedImageManager));
    test_profile_builder.AddTestingFactory(
        HomeBackgroundImageServiceFactory::GetInstance(),
        base::BindRepeating(&CreateBackgroundImageService));
    profile_ = std::move(test_profile_builder).Build();

    mock_image_fetcher_ = std::make_unique<image_fetcher::MockImageFetcher>();

    HomeBackgroundCustomizationService* background_service =
        HomeBackgroundCustomizationServiceFactory::GetForProfile(
            profile_.get());
    HomeBackgroundImageService* home_background_image_service =
        HomeBackgroundImageServiceFactory::GetForProfile(profile_.get());
    UserUploadedImageManager* user_image_manager =
        UserUploadedImageManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[HomeCustomizationBackgroundConfigurationMediator alloc]
        initWithBackgroundCustomizationService:background_service
                                  imageFetcher:mock_image_fetcher_.get()
                    homeBackgroundImageService:home_background_image_service
                      userUploadedImageManager:user_image_manager];

    consumer_ =
        [[FakeHomeCustomizationBackgroundConfigurationConsumer alloc] init];
    mediator_.consumer = consumer_;
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());

    registry->RegisterBooleanPref(prefs::kNTPCustomBackgroundEnabledByPolicy,
                                  true);
    registry->RegisterIntegerPref(themes::prefs::kPolicyThemeColor,
                                  SK_ColorTRANSPARENT);
    return prefs;
  }

  FakeHomeBackgroundImageService* FakeImageService() {
    return static_cast<FakeHomeBackgroundImageService*>(
        HomeBackgroundImageServiceFactory::GetForProfile(profile_.get()));
  }

  FakeUserUploadedImageManager* FakeUserImageManager() {
    return static_cast<FakeUserUploadedImageManager*>(
        UserUploadedImageManagerFactory::GetForProfile(profile_.get()));
  }

  HomeBackgroundCustomizationService* CustomizationService() {
    return HomeBackgroundCustomizationServiceFactory::GetForProfile(
        profile_.get());
  }

  CollectionImage CreateImage(int asset_id, std::string collection_id) {
    CollectionImage image;
    image.collection_id = collection_id;
    image.asset_id = asset_id;

    image.image_url =
        GURL(std::format("http://www.google.com/image{}", asset_id));
    image.thumbnail_image_url = AddOptionsToImageURL(
        image.image_url.spec(), GetThumbnailImageOptions());
    image.attribution = {"Drawn by", "Chrome on iOS"};
    image.attribution_action_url =
        GURL(std::format("http://www.google.com/attribution{}", asset_id));

    return image;
  }

 protected:
  web::WebTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<TestProfileIOS> profile_;

  std::unique_ptr<image_fetcher::MockImageFetcher> mock_image_fetcher_;

  // Mediator being tested by these tests.
  HomeCustomizationBackgroundConfigurationMediator* mediator_;

  // Consumer that the mediator will alert of data changes.
  FakeHomeCustomizationBackgroundConfigurationConsumer* consumer_;
};

// Tests loading gallery backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadGalleryBackgroundConfigurations) {
  // Set up gallery data.
  std::string collection1_name = "Architecture";
  std::vector<CollectionImage> collection1;
  collection1.push_back(CreateImage(1, collection1_name));
  collection1.push_back(CreateImage(2, collection1_name));

  std::string collection2_name = "Patterns";
  std::vector<CollectionImage> collection2;
  collection2.push_back(CreateImage(3, collection2_name));
  collection2.push_back(CreateImage(4, collection2_name));

  HomeBackgroundImageService::CollectionImageMap collection_images;
  collection_images.emplace_back(collection1_name, collection1);
  collection_images.emplace_back(collection2_name, collection2);

  FakeHomeBackgroundImageService* image_service = FakeImageService();
  image_service->SetCollectionData(collection_images);

  // Set image 1 from collection 1 as the current background.
  CustomizationService()->SetCurrentBackground(
      collection1[0].image_url, collection1[0].thumbnail_image_url,
      collection1[0].attribution[0], collection1[0].attribution[1],
      collection1[0].attribution_action_url, collection1[0].collection_id);

  [mediator_ loadGalleryBackgroundConfigurations];

  // Verify collections
  ASSERT_EQ(collection_images.size(), consumer_.configurations.count);

  for (size_t i = 0; i < collection_images.size(); i++) {
    EXPECT_EQ(
        std::get<0>(collection_images[i]),
        base::SysNSStringToUTF8(consumer_.configurations[i].collectionName));

    std::vector<CollectionImage> expected_images =
        std::get<1>(collection_images[i]);

    // Verify that each collection has the right number of items.
    EXPECT_EQ(expected_images.size(),
              consumer_.configurations[i].configurationOrder.count);
    EXPECT_EQ(expected_images.size(),
              consumer_.configurations[i].configurations.count);

    // Verify that each individual item matches the data.
    for (size_t j = 0; j < expected_images.size(); j++) {
      NSString* item_id = consumer_.configurations[i].configurationOrder[j];
      id<BackgroundCustomizationConfiguration> item =
          consumer_.configurations[i].configurations[item_id];
      EXPECT_EQ(HomeCustomizationBackgroundStyle::kPreset,
                item.backgroundStyle);
      EXPECT_EQ(expected_images[j].thumbnail_image_url, item.thumbnailURL);
    }
  }

  // Verify that the item that was set as the current background initially is
  // identified as the currently selected background.
  EXPECT_EQ(consumer_.configurations[0].configurationOrder[0],
            consumer_.selectedBackgroundId);
}

// Tests loading color backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadColorBackgroundConfigurations) {
  [mediator_ loadColorBackgroundConfigurations];

  // Verify collection.
  ASSERT_EQ(1u, consumer_.configurations.count);

  BackgroundCollectionConfiguration* collection = consumer_.configurations[0];

  // First element is the default background, so expected size is number of
  // colors + 1.
  EXPECT_EQ(kSeedColors.size() + 1, collection.configurationOrder.count);
  EXPECT_EQ(kSeedColors.size() + 1, collection.configurations.count);

  for (size_t i = 0; i < kSeedColors.size(); i++) {
    NSString* item_id = collection.configurationOrder[i];
    id<BackgroundCustomizationConfiguration> item =
        collection.configurations[item_id];

    // First item is the default background option.
    if (i == 0) {
      EXPECT_EQ(HomeCustomizationBackgroundStyle::kDefault,
                item.backgroundStyle);
      continue;
    }

    // First element is default, so expected color is at index i - 1.
    SeedColor expected_color = kSeedColors[i - 1];

    EXPECT_EQ(HomeCustomizationBackgroundStyle::kColor, item.backgroundStyle);
    EXPECT_EQ(expected_color.color,
              skia::UIColorToSkColor(item.backgroundColor));
    EXPECT_EQ(expected_color.variant, item.colorVariant);
  }

  // If no color is initially selected, the initial default background
  // configuration should be selected.
  EXPECT_EQ(collection.configurationOrder[0], consumer_.selectedBackgroundId);
}

// Tests that when color backgrounds are loaded, the correct color is selected.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadColorBackgroundConfigurationsSelectedColor) {
  // Set a color as selected.
  int selected_color = 2;
  CustomizationService()->SetBackgroundColor(
      kSeedColors[selected_color].color,
      SchemeVariantToProtoEnum(kSeedColors[selected_color].variant));

  [mediator_ loadColorBackgroundConfigurations];

  // The first item in the configurations is the default color selector.
  EXPECT_EQ(consumer_.configurations[0].configurationOrder[selected_color + 1],
            consumer_.selectedBackgroundId);
}

// Tests loading the recently used backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadRecentlyUsedBackgroundConfigurations) {
  // Set one color, one gallery image, and one user-uploaded image as recently
  // used items.
  SeedColor color = kSeedColors[0];
  CustomizationService()->SetBackgroundColor(
      color.color, SchemeVariantToProtoEnum(color.variant));
  CustomizationService()->StoreCurrentTheme();

  CollectionImage image = CreateImage(1, "Default");
  CustomizationService()->SetCurrentBackground(
      image.image_url, image.thumbnail_image_url, image.attribution[0],
      image.attribution[1], image.attribution_action_url, image.collection_id);
  CustomizationService()->StoreCurrentTheme();

  HomeUserUploadedBackground user_image;
  user_image.image_path = "image.jpg";
  user_image.framing_coordinates = FramingCoordinates(5, 10, 15, 20);
  CustomizationService()->SetCurrentUserUploadedBackground(
      user_image.image_path, user_image.framing_coordinates);
  CustomizationService()->StoreCurrentTheme();

  [mediator_ loadRecentlyUsedBackgroundConfigurations];
  ASSERT_EQ(1u, consumer_.configurations.count);

  BackgroundCollectionConfiguration* collection = consumer_.configurations[0];

  // There should be 4 items: default, then the user image, gallery image, and
  // color (the set backgrounds in reverse order).
  EXPECT_EQ(4u, collection.configurationOrder.count);
  EXPECT_EQ(4u, collection.configurations.count);

  for (NSUInteger i = 0; i < collection.configurations.count; i++) {
    NSString* item_id = collection.configurationOrder[i];
    id<BackgroundCustomizationConfiguration> item =
        collection.configurations[item_id];

    switch (i) {
      // Default.
      case 0:
        EXPECT_EQ(HomeCustomizationBackgroundStyle::kDefault,
                  item.backgroundStyle);
        break;
      // User image.
      case 1:
        EXPECT_EQ(HomeCustomizationBackgroundStyle::kUserUploaded,
                  item.backgroundStyle);
        EXPECT_EQ(user_image.image_path,
                  base::SysNSStringToUTF8(item.userUploadedImagePath));
        EXPECT_EQ(user_image.framing_coordinates,
                  FramingCoordinatesFromHomeCustomizationFramingCoordinates(
                      item.userUploadedFramingCoordinates));
        break;
      // Gallery image.
      case 2:
        EXPECT_EQ(HomeCustomizationBackgroundStyle::kPreset,
                  item.backgroundStyle);
        EXPECT_EQ(image.thumbnail_image_url, item.thumbnailURL);
        break;
      // Color.
      case 3:
        EXPECT_EQ(HomeCustomizationBackgroundStyle::kColor,
                  item.backgroundStyle);
        EXPECT_EQ(color.color, skia::UIColorToSkColor(item.backgroundColor));
        EXPECT_EQ(color.variant, item.colorVariant);
        break;
      default:
        ADD_FAILURE();
        break;
    }
  }

  // Index 1, the user-uploaded image, should be the selected one.
  EXPECT_EQ(collection.configurationOrder[1], consumer_.selectedBackgroundId);
}

// Tests that applying a background configuration based off of a
// `CollectionImage` applies correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       ApplyBackgroundConfiguration_CollectionImage) {
  // Generate data.
  CollectionImage image = CreateImage(1, "Default");

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithCollectionImage:image
                accessibilityName:@""
               accessibilityValue:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  std::optional<HomeCustomBackground> actual =
      CustomizationService()->GetCurrentCustomBackground();
  ASSERT_TRUE(actual);
  ASSERT_TRUE(
      std::holds_alternative<sync_pb::NtpCustomBackground>(actual.value()));
  sync_pb::NtpCustomBackground actual_background =
      std::get<sync_pb::NtpCustomBackground>(actual.value());
  EXPECT_EQ(image.image_url, actual_background.url());
}

// Tests that applying a background configuration based off of a
// `sync_pb::NtpCustomBackground` applies correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       ApplyBackgroundConfiguration_NtpCustomBackground) {
  // Generate data.
  sync_pb::NtpCustomBackground custom_background;
  custom_background.set_url("http://www.google.com/image1");
  custom_background.set_attribution_line_1("Drawn by");
  custom_background.set_attribution_line_2("Chrome on iOS");
  custom_background.set_attribution_action_url(
      "http://www.google.com/attribution1");
  custom_background.set_collection_id("Default");

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithNtpCustomBackground:custom_background
                    accessibilityName:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  std::optional<HomeCustomBackground> actual =
      CustomizationService()->GetCurrentCustomBackground();
  ASSERT_TRUE(actual);
  ASSERT_TRUE(
      std::holds_alternative<sync_pb::NtpCustomBackground>(actual.value()));
  EXPECT_EQ(custom_background,
            std::get<sync_pb::NtpCustomBackground>(actual.value()));
}

// Tests that applying a background configuration based off of a color applies
// correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       ApplyBackgroundConfiguration_Color) {
  // Generate data.
  SeedColor color = kSeedColors[0];

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithBackgroundColor:skia::UIColorFromSkColor(color.color)
                     colorVariant:color.variant
                accessibilityName:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  std::optional<sync_pb::UserColorTheme> actual =
      CustomizationService()->GetCurrentColorTheme();
  ASSERT_TRUE(actual);
  EXPECT_EQ(color.color, actual->color());
  EXPECT_EQ(color.variant,
            ProtoEnumToSchemeVariant(actual->browser_color_variant()));
}

// Tests that applying a background configuration based off of a user image
// applies correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       ApplyBackgroundConfiguration_UserImage) {
  // Generate data.
  HomeUserUploadedBackground user_background;
  user_background.image_path = "image.jpg";
  user_background.framing_coordinates = FramingCoordinates(5, 10, 15, 20);

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithUserUploadedImagePath:base::SysUTF8ToNSString(
                                            user_background.image_path)
                     framingCoordinates:user_background.framing_coordinates
                      accessibilityName:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  std::optional<HomeCustomBackground> actual =
      CustomizationService()->GetCurrentCustomBackground();
  ASSERT_TRUE(actual);
  ASSERT_TRUE(
      std::holds_alternative<HomeUserUploadedBackground>(actual.value()));
  EXPECT_EQ(user_background,
            std::get<HomeUserUploadedBackground>(actual.value()));
}

// Tests that applying the default background configuration applies correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       ApplyBackgroundConfiguration_Default) {
  // First, set the background in the service to something else.
  SeedColor color = kSeedColors[0];
  CustomizationService()->SetBackgroundColor(
      color.color, SchemeVariantToProtoEnum(color.variant));
  EXPECT_TRUE(CustomizationService()->GetCurrentColorTheme());

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  EXPECT_FALSE(CustomizationService()->GetCurrentColorTheme());
}

// Tests that saving the current theme via the mediator after setting a theme
// saves correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest, SaveCurrentTheme) {
  // Generate data.
  CollectionImage image = CreateImage(1, "Default");

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithCollectionImage:image
                accessibilityName:@""
               accessibilityValue:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);

  // Save the current theme, which should then persist the theme in the service.
  [mediator_ saveCurrentTheme];

  EXPECT_FALSE(mediator_.themeHasChanged);

  // Now, even after resetting the theme via the service, the current background
  // should still be the set one.
  CustomizationService()->RestoreCurrentTheme();

  std::optional<HomeCustomBackground> actual =
      CustomizationService()->GetCurrentCustomBackground();
  ASSERT_TRUE(actual);
  ASSERT_TRUE(
      std::holds_alternative<sync_pb::NtpCustomBackground>(actual.value()));
  sync_pb::NtpCustomBackground actual_background =
      std::get<sync_pb::NtpCustomBackground>(actual.value());
  EXPECT_EQ(image.image_url, actual_background.url());
}

// Tests that cancelling the current set theme before saving cancels correctly
// and reverts back to the original theme.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       CancelThemeSelection) {
  // At first, there should be no background.
  EXPECT_FALSE(CustomizationService()->GetCurrentCustomBackground());

  // Generate data.
  HomeUserUploadedBackground user_background;
  user_background.image_path = "image.jpg";
  user_background.framing_coordinates = FramingCoordinates(5, 10, 15, 20);

  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc]
          initWithUserUploadedImagePath:base::SysUTF8ToNSString(
                                            user_background.image_path)
                     framingCoordinates:user_background.framing_coordinates
                      accessibilityName:@""];

  // Ask mediator to apply background.
  [mediator_ applyBackgroundForConfiguration:configuration];

  // Verify that background has been applied and that the mediator is tracking
  // this.
  EXPECT_TRUE(mediator_.themeHasChanged);
  EXPECT_TRUE(CustomizationService()->GetCurrentCustomBackground());

  // Now, cancel the change in the mediator.
  [mediator_ cancelThemeSelection];

  // Background in service should be back to the color.
  EXPECT_FALSE(mediator_.themeHasChanged);
  EXPECT_FALSE(CustomizationService()->GetCurrentCustomBackground());
}

// Tests that the mediator will correctly delete recently backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       DeleteBackgroundFromRecentlyUsed) {
  // First, add one recently used background to the list.
  SeedColor color = kSeedColors[0];
  CustomizationService()->SetBackgroundColor(
      color.color, SchemeVariantToProtoEnum(color.variant));
  CustomizationService()->StoreCurrentTheme();

  CustomizationService()->ClearCurrentBackground();
  CustomizationService()->StoreCurrentTheme();

  ASSERT_EQ(1u, CustomizationService()->GetRecentlyUsedBackgrounds().size());

  // Get the background configuration from the mediator.
  [mediator_ loadRecentlyUsedBackgroundConfigurations];

  // The color option is index 1 in configurationOrder as index 0 is the default
  // background.
  NSString* configuration_id =
      consumer_.configurations[0].configurationOrder[1];
  id<BackgroundCustomizationConfiguration> configuration =
      consumer_.configurations[0].configurations[configuration_id];

  [mediator_ deleteBackgroundFromRecentlyUsed:configuration];

  ASSERT_EQ(0u, CustomizationService()->GetRecentlyUsedBackgrounds().size());
}

// Tests that fetching a thumbnail image where the image fetch is a success
// works correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       FetchThumbnailImage_Success) {
  // Set up call.
  GURL url("https://www.google.com/image");
  gfx::Image expected_image = gfx::test::CreateImage(20, 20);

  EXPECT_CALL(*mock_image_fetcher_.get(),
              FetchImageAndData_(url, testing::_, testing::_, testing::_))
      .WillOnce(PostFetchReply(expected_image, 200));

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  __block UIImage* actual_image;
  __block NSError* actual_error;
  base::HistogramTester histogram_tester;

  [mediator_ fetchBackgroundCustomizationThumbnailURLImage:url
                                                completion:^(UIImage* image,
                                                             NSError* error) {
                                                  actual_image = image;
                                                  actual_error = error;
                                                  run_loop_ptr->Quit();
                                                }];

  run_loop.Run();

  EXPECT_NE(nil, actual_image);
  EXPECT_EQ(nil, actual_error);

  histogram_tester.ExpectUniqueSample(
      "IOS.HomeCustomization.Background.Gallery.ImageDownloadSuccessful", true,
      1);
}

// Tests that fetching a thumbnail image where the image fetch is a failure
// works correctly.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       FetchThumbnailImage_Failure) {
  // Set up call with empty image to indicate failure.
  GURL url("https://www.google.com/image");
  gfx::Image empty_image;

  EXPECT_CALL(*mock_image_fetcher_.get(),
              FetchImageAndData_(url, testing::_, testing::_, testing::_))
      .WillOnce(PostFetchReply(empty_image, 404));

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  __block UIImage* actual_image;
  __block NSError* actual_error;
  base::HistogramTester histogram_tester;

  [mediator_ fetchBackgroundCustomizationThumbnailURLImage:url
                                                completion:^(UIImage* image,
                                                             NSError* error) {
                                                  actual_image = image;
                                                  actual_error = error;
                                                  run_loop_ptr->Quit();
                                                }];

  run_loop.Run();

  EXPECT_EQ(nil, actual_image);
  EXPECT_NE(nil, actual_error);

  histogram_tester.ExpectUniqueSample(
      "IOS.HomeCustomization.Background.Gallery.ImageDownloadSuccessful", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "IOS.HomeCustomization.Background.Gallery.ImageDownloadErrorCode", 404,
      1);
}

// Tests that the mediator correctly loads the user-uploaded background images.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       FetchBackgroundCustomizationUserUploadedImage) {
  // First, make sure the user image manager contains the image.
  UIImage* expected_image = gfx::test::CreateImage(20, 20).ToUIImage();
  base::FilePath image_path =
      FakeUserImageManager()->StoreUserUploadedImage(expected_image);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  __block UIImage* actual_image;

  [mediator_
      fetchBackgroundCustomizationUserUploadedImage:base::SysUTF8ToNSString(
                                                        image_path.value())
                                         completion:^(
                                             UIImage* image,
                                             UserUploadedImageError error) {
                                           actual_image = image;
                                           run_loop_ptr->Quit();
                                         }];

  run_loop.Run();

  EXPECT_EQ(expected_image, actual_image);
}
