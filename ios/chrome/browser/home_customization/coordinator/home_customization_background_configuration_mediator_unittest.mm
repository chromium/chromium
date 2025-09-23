// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_configuration_mediator.h"

#import "base/files/scoped_temp_dir.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
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

namespace {

std::unique_ptr<KeyedService> CreateBackgroundImageService(
    ProfileIOS* profile) {
  return std::make_unique<FakeHomeBackgroundImageService>(
      NtpBackgroundServiceFactory::GetForProfile(profile));
}

std::unique_ptr<KeyedService> CreateUserUploadedImageManager(
    const base::FilePath& path,
    ProfileIOS* profile) {
  return std::make_unique<UserUploadedImageManager>(
      path, base::SequencedTaskRunner::GetCurrentDefault());
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

@end

// Tests for the Home Customization Background Configuration mediator.
class HomeCustomizationBackgroundConfigurationMediatorTest
    : public PlatformTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kNTPBackgroundCustomization);

    ASSERT_TRUE(image_dir_.CreateUniqueTempDir());

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.SetPrefService(CreatePrefService());
    test_profile_builder.AddTestingFactory(
        UserUploadedImageManagerFactory::GetInstance(),
        base::BindRepeating(&CreateUserUploadedImageManager,
                            image_dir_.GetPath()));
    test_profile_builder.AddTestingFactory(
        HomeBackgroundImageServiceFactory::GetInstance(),
        base::BindRepeating(&CreateBackgroundImageService));
    profile_ = std::move(test_profile_builder).Build();

    HomeBackgroundCustomizationService* background_service =
        HomeBackgroundCustomizationServiceFactory::GetForProfile(
            profile_.get());
    image_fetcher::ImageFetcherService* image_fetcher =
        ImageFetcherServiceFactory::GetForProfile(profile_.get());
    HomeBackgroundImageService* home_background_image_service =
        HomeBackgroundImageServiceFactory::GetForProfile(profile_.get());
    UserUploadedImageManager* user_image_manager =
        UserUploadedImageManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[HomeCustomizationBackgroundConfigurationMediator alloc]
        initWithBackgroundCustomizationService:background_service
                           imageFetcherService:image_fetcher
                    homeBackgroundImageService:home_background_image_service
                      userUploadedImageManager:user_image_manager];

    configuration_consumer_ =
        [[FakeHomeCustomizationBackgroundConfigurationConsumer alloc] init];
    mediator_.configurationConsumer = configuration_consumer_;
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

  // Temp dir for the UserUploadedImageManager.
  base::ScopedTempDir image_dir_;

  std::unique_ptr<TestProfileIOS> profile_;

  // Mediator being tested by these tests.
  HomeCustomizationBackgroundConfigurationMediator* mediator_;

  // Confguration Consumer that the mediator will alert of data changes.
  FakeHomeCustomizationBackgroundConfigurationConsumer* configuration_consumer_;
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
  HomeBackgroundCustomizationService* background_service =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile_.get());
  background_service->SetCurrentBackground(
      collection1[0].image_url, collection1[0].thumbnail_image_url,
      collection1[0].attribution[0], collection1[0].attribution[1],
      collection1[0].attribution_action_url, collection1[0].collection_id);

  [mediator_ loadGalleryBackgroundConfigurations];

  // Verify collections
  ASSERT_EQ(collection_images.size(),
            configuration_consumer_.configurations.count);

  for (size_t i = 0; i < collection_images.size(); i++) {
    EXPECT_EQ(std::get<0>(collection_images[i]),
              base::SysNSStringToUTF8(
                  configuration_consumer_.configurations[i].collectionName));

    std::vector<CollectionImage> expected_images =
        std::get<1>(collection_images[i]);

    // Verify that each collection has the right number of items.
    EXPECT_EQ(
        expected_images.size(),
        configuration_consumer_.configurations[i].configurationOrder.count);
    EXPECT_EQ(expected_images.size(),
              configuration_consumer_.configurations[i].configurations.count);

    // Verify that each individual item matches the data.
    for (size_t j = 0; j < expected_images.size(); j++) {
      NSString* item_id =
          configuration_consumer_.configurations[i].configurationOrder[j];
      id<BackgroundCustomizationConfiguration> item =
          configuration_consumer_.configurations[i].configurations[item_id];
      EXPECT_EQ(HomeCustomizationBackgroundStyle::kPreset,
                item.backgroundStyle);
      EXPECT_EQ(expected_images[j].thumbnail_image_url, item.thumbnailURL);
    }
  }

  // Verify that the item that was set as the current background initially is
  // identified as the currently selected background.
  EXPECT_EQ(configuration_consumer_.configurations[0].configurationOrder[0],
            configuration_consumer_.selectedBackgroundId);
}

// Tests loading color backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadColorBackgroundConfigurations) {
  [mediator_ loadColorBackgroundConfigurations];

  // Verify collection.
  ASSERT_EQ(1u, configuration_consumer_.configurations.count);

  BackgroundCollectionConfiguration* collection =
      configuration_consumer_.configurations[0];

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
  EXPECT_EQ(collection.configurationOrder[0],
            configuration_consumer_.selectedBackgroundId);
}

// Tests that when color backgrounds are loaded, the correct color is selected.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadColorBackgroundConfigurationsSelectedColor) {
  // Set a color as selected.
  int selected_color = 2;
  HomeBackgroundCustomizationService* background_service =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile_.get());
  background_service->SetBackgroundColor(
      kSeedColors[selected_color].color,
      SchemeVariantToProtoEnum(kSeedColors[selected_color].variant));

  [mediator_ loadColorBackgroundConfigurations];

  // The first item in the configurations is the default color selector.
  EXPECT_EQ(configuration_consumer_.configurations[0]
                .configurationOrder[selected_color + 1],
            configuration_consumer_.selectedBackgroundId);
}

// Tests loading the recently used backgrounds.
TEST_F(HomeCustomizationBackgroundConfigurationMediatorTest,
       LoadRecentlyUsedBackgroundConfigurations) {
  HomeBackgroundCustomizationService* background_service =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile_.get());

  // Set one color, one gallery image, and one user-uploaded image as recently
  // used items.
  SeedColor color = kSeedColors[0];
  background_service->SetBackgroundColor(
      color.color, SchemeVariantToProtoEnum(color.variant));
  background_service->StoreCurrentTheme();

  CollectionImage image = CreateImage(1, "Default");
  background_service->SetCurrentBackground(
      image.image_url, image.thumbnail_image_url, image.attribution[0],
      image.attribution[1], image.attribution_action_url, image.collection_id);
  background_service->StoreCurrentTheme();

  HomeUserUploadedBackground user_image;
  user_image.image_path = "image.jpg";
  user_image.framing_coordinates = FramingCoordinates(5, 10, 15, 20);
  background_service->SetCurrentUserUploadedBackground(
      user_image.image_path, user_image.framing_coordinates);
  background_service->StoreCurrentTheme();

  [mediator_ loadRecentlyUsedBackgroundConfigurations];
  ASSERT_EQ(1u, configuration_consumer_.configurations.count);

  BackgroundCollectionConfiguration* collection =
      configuration_consumer_.configurations[0];

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
  EXPECT_EQ(collection.configurationOrder[1],
            configuration_consumer_.selectedBackgroundId);
}
