// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import "base/files/scoped_temp_dir.h"
#import "base/scoped_observation.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync/protocol/theme_specifics.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class StubHomeBackgroundCustomizationServiceObserver
    : public HomeBackgroundCustomizationServiceObserver {
 public:
  void OnBackgroundChanged() override { on_background_changed_called = true; }

  bool on_background_changed_called = false;
};

}  // namespace

class HomeBackgroundCustomizationServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kNTPBackgroundCustomization);

    ASSERT_TRUE(image_dir_.CreateUniqueTempDir());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    application_locale_storage_ = std::make_unique<ApplicationLocaleStorage>();
    ntp_background_service_ = std::make_unique<NtpBackgroundService>(
        application_locale_storage_.get(), test_shared_loader_factory_);

    user_image_manager_ = std::make_unique<UserUploadedImageManager>(
        image_dir_.GetPath(), base::SequencedTaskRunner::GetCurrentDefault());
    background_image_service_ =
        std::make_unique<FakeHomeBackgroundImageService>(
            ntp_background_service_.get());

    background_image_service_->SetDefaultCollectionData(
        GetDefaultCollectionImages());

    HomeBackgroundCustomizationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kNTPCustomBackgroundEnabledByPolicy, true);
  }

  void TearDown() override {
    ntp_background_service_->Shutdown();
    ntp_background_service_.reset();
  }

  void CreateService() {
    service_ = std::make_unique<HomeBackgroundCustomizationService>(
        pref_service_.get(), user_image_manager_.get(),
        background_image_service_.get());
    observation_.Observe(service_.get());
  }

  sync_pb::ThemeSpecificsIos DecodeThemeSpecificsIos(std::string encoded) {
    // This pref is base64 encoded, so decode it first.
    std::string serialized;
    base::Base64Decode(encoded, &serialized);
    sync_pb::ThemeSpecificsIos theme_specifics_ios;
    theme_specifics_ios.ParseFromString(serialized);
    return theme_specifics_ios;
  }

  std::vector<CollectionImage> GetDefaultRecentlyUsedImages() {
    CollectionImage image;
    image.collection_id = "Default";
    image.asset_id = 1;
    image.thumbnail_image_url = GURL("http://www.google.com/thumbnail");
    image.image_url = GURL("http://www.google.com/image");
    image.attribution = {"Drawn by", "Chrome on iOS"};
    image.attribution_action_url = GURL("http://www.google.com/attribution");

    return {image};
  }

  HomeBackgroundImageService::CollectionImageMap GetDefaultCollectionImages() {
    std::tuple<std::string, std::vector<CollectionImage>> collection =
        std::make_tuple("Default", GetDefaultRecentlyUsedImages());

    return {collection};
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;

  base::ScopedTempDir image_dir_;

  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<NtpBackgroundService> ntp_background_service_;

  std::unique_ptr<UserUploadedImageManager> user_image_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<FakeHomeBackgroundImageService> background_image_service_;

  std::unique_ptr<HomeBackgroundCustomizationService> service_;

  StubHomeBackgroundCustomizationServiceObserver observer_;
  base::ScopedObservation<HomeBackgroundCustomizationService,
                          StubHomeBackgroundCustomizationServiceObserver>
      observation_{&observer_};
};

// Tests that when the service is initialized, it loads and stores the default
// collection of backgrounds.
TEST_F(HomeBackgroundCustomizationServiceTest,
       TestLoadsRecentlyUsedBackgroundsOnFirstLoad) {
  CreateService();

  const base::Value::List& recently_used_backgrounds =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_EQ(GetDefaultRecentlyUsedImages().size(),
            recently_used_backgrounds.size());

  for (size_t i = 0; i < GetDefaultRecentlyUsedImages().size(); i++) {
    CollectionImage expected = GetDefaultRecentlyUsedImages()[i];
    const base::Value& actual = recently_used_backgrounds[i];

    EXPECT_TRUE(actual.is_string());

    sync_pb::ThemeSpecificsIos theme_specifics =
        DecodeThemeSpecificsIos(actual.GetString());

    EXPECT_EQ(expected.image_url, theme_specifics.ntp_background().url());
    EXPECT_EQ(expected.attribution_action_url,
              theme_specifics.ntp_background().attribution_action_url());
    EXPECT_EQ(expected.attribution[0],
              theme_specifics.ntp_background().attribution_line_1());
    EXPECT_EQ(expected.attribution[1],
              theme_specifics.ntp_background().attribution_line_2());
  }
}

// Tests that when the service is initialized, it does not load or store the
// default collection of backgrounds if recently used backgrounds have already
// been stored.
TEST_F(HomeBackgroundCustomizationServiceTest,
       TestDoesntLoadRecentlyUsedBackgroundsIfAlreadyLoaded) {
  // Override the default value of this pref to alert the service that the
  // recently used backgrounds list has been loaded in the past.
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds, {});
  CreateService();

  const base::Value::List& recently_used_backgrounds =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_EQ(0u, recently_used_backgrounds.size());
}

// Tests that setting and then persisting a gallery background works correctly
// and updates the correct in-memory and disk data, including the recently used
// list.
TEST_F(HomeBackgroundCustomizationServiceTest, SetCurrentBackground) {
  CreateService();

  GURL background_url = GURL("http://www.google.com/myImage");
  std::string attribution_line_1 = "Drawn by";
  std::string attribution_line_2 = "Chrome on iOS";
  GURL attribution_action_url = GURL("http://www.google.com/action");
  std::string collection_id = "Default";

  service_->SetCurrentBackground(background_url, GURL(), attribution_line_1,
                                 attribution_line_2, attribution_action_url,
                                 collection_id);

  EXPECT_TRUE(observer_.on_background_changed_called);

  // First make sure that active current background is the provided one.
  std::optional<HomeCustomBackground> current_background =
      service_->GetCurrentCustomBackground();
  ASSERT_TRUE(current_background);
  ASSERT_TRUE(std::holds_alternative<sync_pb::NtpCustomBackground>(
      current_background.value()));
  sync_pb::NtpCustomBackground custom_background =
      std::get<sync_pb::NtpCustomBackground>(current_background.value());

  // Color should not exist after background is set.
  EXPECT_FALSE(service_->GetCurrentColorTheme());

  EXPECT_EQ(background_url, custom_background.url());
  EXPECT_EQ(attribution_line_1, custom_background.attribution_line_1());
  EXPECT_EQ(attribution_line_2, custom_background.attribution_line_2());
  EXPECT_EQ(attribution_action_url, custom_background.attribution_action_url());
  EXPECT_EQ(collection_id, custom_background.collection_id());

  // Now persist background to disk.
  service_->StoreCurrentTheme();

  // Make sure disk data has this item.
  sync_pb::ThemeSpecificsIos disk_theme_specifics = DecodeThemeSpecificsIos(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  EXPECT_EQ(background_url, disk_theme_specifics.ntp_background().url());
  EXPECT_EQ(attribution_line_1,
            disk_theme_specifics.ntp_background().attribution_line_1());
  EXPECT_EQ(attribution_line_2,
            disk_theme_specifics.ntp_background().attribution_line_2());
  EXPECT_EQ(attribution_action_url,
            disk_theme_specifics.ntp_background().attribution_action_url());
  EXPECT_EQ(collection_id,
            disk_theme_specifics.ntp_background().collection_id());

  EXPECT_TRUE(
      pref_service_->GetDict(prefs::kIosUserUploadedBackground).empty());

  // Make sure recent backgrounds disk data has this item first.
  const base::Value::List& recent_backgrounds_disk =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_GE(recent_backgrounds_disk.size(), 1u);

  const base::Value& value = recent_backgrounds_disk[0];
  ASSERT_TRUE(value.is_string());
  sync_pb::ThemeSpecificsIos recent_theme_specifics_disk =
      DecodeThemeSpecificsIos(value.GetString());

  EXPECT_EQ(background_url, recent_theme_specifics_disk.ntp_background().url());
  EXPECT_EQ(attribution_line_1,
            recent_theme_specifics_disk.ntp_background().attribution_line_1());
  EXPECT_EQ(attribution_line_2,
            recent_theme_specifics_disk.ntp_background().attribution_line_2());
  EXPECT_EQ(
      attribution_action_url,
      recent_theme_specifics_disk.ntp_background().attribution_action_url());
  EXPECT_EQ(collection_id,
            recent_theme_specifics_disk.ntp_background().collection_id());

  // Make sure recent backgrounds in-memory data has this item first.
  std::vector<RecentlyUsedBackground> recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();
  ASSERT_GE(recent_backgrounds.size(), 1u);
  RecentlyUsedBackground recent_background = recent_backgrounds[0];

  ASSERT_TRUE(std::holds_alternative<HomeCustomBackground>(recent_background));
  HomeCustomBackground recent_home_custom_background =
      std::get<HomeCustomBackground>(recent_background);
  ASSERT_TRUE(std::holds_alternative<sync_pb::NtpCustomBackground>(
      recent_home_custom_background));
  sync_pb::NtpCustomBackground recent_ntp_custom_background =
      std::get<sync_pb::NtpCustomBackground>(recent_home_custom_background);

  EXPECT_EQ(background_url, recent_ntp_custom_background.url());
  EXPECT_EQ(attribution_line_1,
            recent_ntp_custom_background.attribution_line_1());
  EXPECT_EQ(attribution_line_2,
            recent_ntp_custom_background.attribution_line_2());
  EXPECT_EQ(attribution_action_url,
            recent_ntp_custom_background.attribution_action_url());
  EXPECT_EQ(collection_id, recent_ntp_custom_background.collection_id());
}

// Tests that setting and then persisting a color background works correctly and
// updates the correct in-memory and disk data, including the recently used
// list.
TEST_F(HomeBackgroundCustomizationServiceTest, SetBackgroundColor) {
  CreateService();

  SkColor color = 0xffff00;
  sync_pb::UserColorTheme::BrowserColorVariant color_variant =
      sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT;

  service_->SetBackgroundColor(color, color_variant);

  EXPECT_TRUE(observer_.on_background_changed_called);

  // First make sure that active current background is the provided one.
  std::optional<sync_pb::UserColorTheme> current_theme =
      service_->GetCurrentColorTheme();
  ASSERT_TRUE(current_theme);

  // Background should not exist after color is set.
  EXPECT_FALSE(service_->GetCurrentCustomBackground());

  EXPECT_EQ(color, current_theme->color());
  EXPECT_EQ(color_variant, current_theme->browser_color_variant());

  // Now persist background to disk.
  service_->StoreCurrentTheme();

  // Make sure disk data has this item.
  sync_pb::ThemeSpecificsIos disk_theme_specifics = DecodeThemeSpecificsIos(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  EXPECT_EQ(color, disk_theme_specifics.user_color_theme().color());
  EXPECT_EQ(color_variant,
            disk_theme_specifics.user_color_theme().browser_color_variant());

  EXPECT_TRUE(
      pref_service_->GetDict(prefs::kIosUserUploadedBackground).empty());

  // Make sure recent backgrounds disk data has this item first.
  const base::Value::List& recent_backgrounds_disk =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_GE(recent_backgrounds_disk.size(), 1u);

  const base::Value& value = recent_backgrounds_disk[0];
  ASSERT_TRUE(value.is_string());
  sync_pb::ThemeSpecificsIos recent_theme_specifics_disk =
      DecodeThemeSpecificsIos(value.GetString());

  EXPECT_EQ(color, recent_theme_specifics_disk.user_color_theme().color());
  EXPECT_EQ(
      color_variant,
      recent_theme_specifics_disk.user_color_theme().browser_color_variant());

  // Make sure recent backgrounds in-memory data has this item first.
  std::vector<RecentlyUsedBackground> recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();
  ASSERT_GE(recent_backgrounds.size(), 1u);
  RecentlyUsedBackground recent_background = recent_backgrounds[0];

  ASSERT_TRUE(
      std::holds_alternative<sync_pb::UserColorTheme>(recent_background));
  sync_pb::UserColorTheme recent_user_color_theme =
      std::get<sync_pb::UserColorTheme>(recent_background);

  EXPECT_EQ(color, recent_user_color_theme.color());
  EXPECT_EQ(color_variant, recent_user_color_theme.browser_color_variant());
}

// Tests that setting and then persisting a user-provided background works
// correctly and updates the correct in-memory and disk data, including the
// recently used list.
TEST_F(HomeBackgroundCustomizationServiceTest, SetUserUploadedBackground) {
  CreateService();

  HomeUserUploadedBackground expected;
  expected.image_path = "test_file.jpg";
  expected.framing_coordinates = FramingCoordinates(5, 10, 25, 50);

  service_->SetCurrentUserUploadedBackground(expected.image_path,
                                             expected.framing_coordinates);

  EXPECT_TRUE(observer_.on_background_changed_called);

  // First make sure that active current background is the provided one.
  std::optional<HomeCustomBackground> current_background =
      service_->GetCurrentCustomBackground();
  ASSERT_TRUE(current_background);
  ASSERT_TRUE(std::holds_alternative<HomeUserUploadedBackground>(
      current_background.value()));
  HomeUserUploadedBackground user_uploaded_background =
      std::get<HomeUserUploadedBackground>(current_background.value());

  // Color should not exist after background is set.
  EXPECT_FALSE(service_->GetCurrentColorTheme());

  EXPECT_EQ(expected, user_uploaded_background);

  // Now persist background to disk.
  service_->StoreCurrentTheme();

  // Make sure disk data has this item.
  std::optional<HomeUserUploadedBackground> disk_user_uploaded_background =
      HomeUserUploadedBackground::FromDict(
          pref_service_->GetDict(prefs::kIosUserUploadedBackground));

  EXPECT_EQ(expected, disk_user_uploaded_background);

  EXPECT_EQ("", pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  // Make sure recent backgrounds disk data has this item first.
  const base::Value::List& recent_backgrounds_disk =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_GE(recent_backgrounds_disk.size(), 1u);

  const base::Value& value = recent_backgrounds_disk[0];
  ASSERT_TRUE(value.is_dict());
  std::optional<HomeUserUploadedBackground>
      recent_user_uploaded_background_disk =
          HomeUserUploadedBackground::FromDict(value.GetDict());

  EXPECT_EQ(expected, recent_user_uploaded_background_disk);

  // Make sure recent backgrounds in-memory data has this item first.
  std::vector<RecentlyUsedBackground> recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();
  ASSERT_GE(recent_backgrounds.size(), 1u);
  RecentlyUsedBackground recent_background = recent_backgrounds[0];

  ASSERT_TRUE(std::holds_alternative<HomeCustomBackground>(recent_background));
  HomeCustomBackground recent_home_custom_background =
      std::get<HomeCustomBackground>(recent_background);
  ASSERT_TRUE(std::holds_alternative<HomeUserUploadedBackground>(
      recent_home_custom_background));
  HomeUserUploadedBackground recent_user_uploaded_background =
      std::get<HomeUserUploadedBackground>(recent_home_custom_background);

  EXPECT_EQ(expected, recent_user_uploaded_background);
}

// Tests that clearing and then persisting that change works correctly and
// updates the correct in-memory and disk data, including the recently used
// list.
TEST_F(HomeBackgroundCustomizationServiceTest, ClearCurrentBackground) {
  // Set the recently used backgrounds list to empty, to make it easier to test
  // that clearing the current background does not add anything to the recent
  // list.
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds, {});
  CreateService();

  // First, set background to something else.
  SkColor color = 0xffff00;
  sync_pb::UserColorTheme::BrowserColorVariant color_variant =
      sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT;

  service_->SetBackgroundColor(color, color_variant);

  EXPECT_TRUE(service_->GetCurrentColorTheme());

  EXPECT_TRUE(observer_.on_background_changed_called);
  observer_.on_background_changed_called = false;

  // Now reset background.
  service_->ClearCurrentBackground();
  EXPECT_TRUE(observer_.on_background_changed_called);

  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_FALSE(service_->GetCurrentColorTheme());

  // Persist changes to disk
  service_->StoreCurrentTheme();

  EXPECT_TRUE(
      pref_service_->GetDict(prefs::kIosUserUploadedBackground).empty());
  EXPECT_EQ("", pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));
  EXPECT_TRUE(
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds).empty());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());
}
