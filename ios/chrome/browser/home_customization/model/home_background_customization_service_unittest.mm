// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

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
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image_unittest_util.h"

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

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

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

    background_image_service_->SetDefaultCollectionData(
        GetDefaultCollectionImages());

    HomeBackgroundCustomizationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kNTPCustomBackgroundEnabledByPolicy, true);
    pref_service_->registry()->RegisterIntegerPref(
        themes::prefs::kPolicyThemeColor, SK_ColorTRANSPARENT);
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

  std::string EncodeThemeSpecificsIos(
      sync_pb::ThemeSpecificsIos theme_specifics_ios) {
    std::string serialized = theme_specifics_ios.SerializeAsString();
    // Encode bytestring so it can be stored in a pref.
    return base::Base64Encode(serialized);
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

  sync_pb::UserColorTheme GenerateUserColorTheme(SkColor color) {
    sync_pb::UserColorTheme color_theme;
    color_theme.set_color(color);
    color_theme.set_browser_color_variant(
        sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT);
    return color_theme;
  }

  HomeUserUploadedBackground GenerateHomeUserUploadedBackground() {
    HomeUserUploadedBackground background;
    background.image_path = "image.jpg";
    background.framing_coordinates = FramingCoordinates(5, 10, 15, 20);
    return background;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<NtpBackgroundService> ntp_background_service_;

  std::unique_ptr<FakeUserUploadedImageManager> user_image_manager_;
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

  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xffff00);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());

  EXPECT_TRUE(observer_.on_background_changed_called);

  // First make sure that active current background is the provided one.
  std::optional<sync_pb::UserColorTheme> current_theme =
      service_->GetCurrentColorTheme();
  ASSERT_TRUE(current_theme);

  // Background should not exist after color is set.
  EXPECT_FALSE(service_->GetCurrentCustomBackground());

  EXPECT_EQ(color_theme, current_theme);

  // Now persist background to disk.
  service_->StoreCurrentTheme();

  // Make sure disk data has this item.
  sync_pb::ThemeSpecificsIos disk_theme_specifics = DecodeThemeSpecificsIos(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  EXPECT_EQ(color_theme, disk_theme_specifics.user_color_theme());

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

  EXPECT_EQ(color_theme, recent_theme_specifics_disk.user_color_theme());

  // Make sure recent backgrounds in-memory data has this item first.
  std::vector<RecentlyUsedBackground> recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();
  ASSERT_GE(recent_backgrounds.size(), 1u);
  RecentlyUsedBackground recent_background = recent_backgrounds[0];

  ASSERT_TRUE(
      std::holds_alternative<sync_pb::UserColorTheme>(recent_background));
  sync_pb::UserColorTheme recent_user_color_theme =
      std::get<sync_pb::UserColorTheme>(recent_background);

  EXPECT_EQ(color_theme, recent_user_color_theme);
}

// Tests that setting and then persisting a user-provided background works
// correctly and updates the correct in-memory and disk data, including the
// recently used list.
TEST_F(HomeBackgroundCustomizationServiceTest, SetUserUploadedBackground) {
  CreateService();

  HomeUserUploadedBackground expected = GenerateHomeUserUploadedBackground();

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
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xffff00);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  ASSERT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());
  EXPECT_EQ(1u,
            pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds).size());
  EXPECT_EQ(1u, service_->GetRecentlyUsedBackgrounds().size());

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

  // Recently used in-memory and disk data should still be size 1.
  EXPECT_EQ(1u,
            pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds).size());
  EXPECT_EQ(1u, service_->GetRecentlyUsedBackgrounds().size());
}

// Tests that setting the background without calling StoreCurrentTheme only
// changes the in-memory data. And then once the theme is reset, the disk
// data is active again.
TEST_F(HomeBackgroundCustomizationServiceTest, SetAndClearTemporaryBackground) {
  // Override the default value of this pref to alert the service that the
  // recently used backgrounds list has been loaded in the past, so the list
  // starts off empty.
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds, {});
  CreateService();

  // First, set and persist the background to a color. This is tested elsewhere.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  std::string disk_theme =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);
  const base::Value::List& initial_recent_backgrounds =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  // Now, set the background temporarily to a user-uploaded image.
  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();

  service_->SetCurrentUserUploadedBackground(
      user_background.image_path, user_background.framing_coordinates);
  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_TRUE(service_->GetCurrentCustomBackground());

  // But, disk data should not change.
  EXPECT_EQ(disk_theme,
            pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));
  EXPECT_EQ(initial_recent_backgrounds,
            pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds));

  // Now, reset the change.
  service_->RestoreCurrentTheme();

  ASSERT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_EQ(disk_theme,
            pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));
  EXPECT_EQ(initial_recent_backgrounds,
            pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds));
}

// Tests that loading the recently used backgrounds from the disk data loads
// correctly.
TEST_F(HomeBackgroundCustomizationServiceTest, LoadRecentBackgrounds) {
  // First, set up data on disk.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  sync_pb::ThemeSpecificsIos color_theme_specifics;
  *color_theme_specifics.mutable_user_color_theme() = color_theme;

  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();

  base::Value::List recent_backgrounds_data =
      base::Value::List()
          .Append(EncodeThemeSpecificsIos(color_theme_specifics))
          .Append(user_background.ToDict());

  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds,
                         std::move(recent_backgrounds_data));

  // Create service to load list from disk.
  CreateService();

  // Make sure that loaded data matches what was persisted to disk.
  std::vector<RecentlyUsedBackground> recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();

  ASSERT_EQ(2u, recent_backgrounds.size());

  ASSERT_TRUE(
      std::holds_alternative<sync_pb::UserColorTheme>(recent_backgrounds[0]));
  EXPECT_EQ(color_theme,
            std::get<sync_pb::UserColorTheme>(recent_backgrounds[0]));

  ASSERT_TRUE(
      std::holds_alternative<HomeCustomBackground>(recent_backgrounds[1]));
  HomeCustomBackground custom_background =
      std::get<HomeCustomBackground>(recent_backgrounds[1]);
  ASSERT_TRUE(
      std::holds_alternative<HomeUserUploadedBackground>(custom_background));
  EXPECT_EQ(user_background,
            std::get<HomeUserUploadedBackground>(custom_background));
}

// Tests that deleting a recently used background correctly removes it from the
// list. Also tests that the recent backgrounds are listed in reverse order of
// use.
TEST_F(HomeBackgroundCustomizationServiceTest, DeleteRecentBackground) {
  // Override the default value of this pref to alert the service that the
  // recently used backgrounds list has been loaded in the past, so the list
  // starts off empty.
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds, {});
  CreateService();

  // Add 2 recent backgrounds.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();

  service_->SetCurrentUserUploadedBackground(
      user_background.image_path, user_background.framing_coordinates);
  service_->StoreCurrentTheme();

  std::vector<RecentlyUsedBackground> initial_recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();

  ASSERT_EQ(2u, initial_recent_backgrounds.size());

  // Make sure that the first item is the user uploaded background.
  ASSERT_TRUE(std::holds_alternative<HomeCustomBackground>(
      initial_recent_backgrounds[0]));
  HomeCustomBackground recent_custom_background =
      std::get<HomeCustomBackground>(initial_recent_backgrounds[0]);
  ASSERT_TRUE(std::holds_alternative<HomeUserUploadedBackground>(
      recent_custom_background));
  EXPECT_EQ(user_background,
            std::get<HomeUserUploadedBackground>(recent_custom_background));

  // Make sure that the second item is the color background.
  ASSERT_TRUE(std::holds_alternative<sync_pb::UserColorTheme>(
      initial_recent_backgrounds[1]));
  EXPECT_EQ(color_theme,
            std::get<sync_pb::UserColorTheme>(initial_recent_backgrounds[1]));

  // Delete the second item.
  service_->DeleteRecentlyUsedBackground(initial_recent_backgrounds[1]);

  // Check that the second item, the color background, is gone.
  std::vector<RecentlyUsedBackground> final_recent_backgrounds =
      service_->GetRecentlyUsedBackgrounds();

  ASSERT_EQ(1u, final_recent_backgrounds.size());
  ASSERT_FALSE(std::holds_alternative<sync_pb::UserColorTheme>(
      final_recent_backgrounds[0]));

  // Make sure that the first item is still the user uploaded background.
  ASSERT_TRUE(std::holds_alternative<HomeCustomBackground>(
      initial_recent_backgrounds[0]));
  recent_custom_background =
      std::get<HomeCustomBackground>(initial_recent_backgrounds[0]);
  ASSERT_TRUE(std::holds_alternative<HomeUserUploadedBackground>(
      recent_custom_background));
  EXPECT_EQ(user_background,
            std::get<HomeUserUploadedBackground>(recent_custom_background));
}

// Tests that setting the `kNTPCustomBackgroundEnabledByPolicy` policy bypasses
// any set background while active and prevents changing the background. And
// then once the policy is disabled, the initial background returns.
TEST_F(HomeBackgroundCustomizationServiceTest,
       CustomBackgroundEnabledEnterprisePolicy) {
  // First, set up some state.
  CreateService();

  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  std::string disk_theme =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);
  EXPECT_NE("", disk_theme);

  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetRecentlyUsedBackgrounds().empty());

  // Now, change policy.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, false);

  // When customization is disabled, all accessors should be empty.
  EXPECT_TRUE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // Setting a background manually shouldn't change anything either.
  sync_pb::UserColorTheme color_theme2 = GenerateUserColorTheme(0x0000ff);

  service_->SetBackgroundColor(color_theme2.color(),
                               color_theme2.browser_color_variant());
  service_->StoreCurrentTheme();

  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // Data on disk should also not have changed.
  EXPECT_EQ(disk_theme,
            pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  // Setting a custom background manually shouldn't change anything.
  service_->SetCurrentBackground(
      GURL("https://www.google.com/test"), GURL(), "Drawn by", "Chrome on iOS",
      GURL("https://www.google.com/action"), "default");

  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // Data on disk should also not have changed.
  EXPECT_EQ(disk_theme,
            pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  // Setting a user-uploaded background shouldn't change anything.
  service_->SetCurrentUserUploadedBackground("image.jpg",
                                             FramingCoordinates(5, 10, 15, 20));

  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // Data on disk should also not have changed.
  EXPECT_EQ(disk_theme,
            pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  // After re-enabling the policy, the correct state should be back.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, true);
  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_EQ(2u, service_->GetRecentlyUsedBackgrounds().size());
}

// Tests that setting the `kPolicyThemeColor` policy bypasses
// any set background while active, overriding it with the given theme color,
// and prevents changing the background. And then once the policy is disabled,
// the initial background returns.
TEST_F(HomeBackgroundCustomizationServiceTest, PolicyThemeColor) {
  // First, set up some state.
  CreateService();

  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());

  // Set managed pref.
  SkColor managed_color = 0x0000ff;
  pref_service_->SetManagedPref(themes::prefs::kPolicyThemeColor,
                                base::Value(static_cast<int>(managed_color)));

  EXPECT_TRUE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(managed_color, service_->GetCurrentColorTheme()->color());

  // Setting the color manually shouldn't change anything.
  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());

  EXPECT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(managed_color, service_->GetCurrentColorTheme()->color());

  // Setting a custom background manually shouldn't change anything.
  service_->SetCurrentBackground(
      GURL("https://www.google.com/test"), GURL(), "Drawn by", "Chrome on iOS",
      GURL("https://www.google.com/action"), "default");

  EXPECT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(managed_color, service_->GetCurrentColorTheme()->color());

  // Setting a user-uploaded background shouldn't change anything.
  service_->SetCurrentUserUploadedBackground("image.jpg",
                                             FramingCoordinates(5, 10, 15, 20));

  EXPECT_EQ(managed_color, service_->GetCurrentColorTheme()->color());

  // Un-manage the pref.
  pref_service_->RemoveManagedPref(themes::prefs::kPolicyThemeColor);

  // Data should be back to the start.
  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());
}

// Tests that the 2 enterprise policies work correctly when both are set.
TEST_F(HomeBackgroundCustomizationServiceTest,
       CustomBackgroundEnabledAndPolicyThemeColor) {
  CreateService();

  // Set the initial theme to a color.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);

  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());

  // Disable the kNTPCustomBackgroundEnabledByPolicy and set a policy theme
  // color.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, false);

  SkColor managed_color = 0x0000ff;
  pref_service_->SetManagedPref(themes::prefs::kPolicyThemeColor,
                                base::Value(static_cast<int>(managed_color)));

  // The policy theme color should be ignored and there should be no active
  // theme.
  EXPECT_TRUE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // Setting anything should do nothing.
  service_->SetCurrentUserUploadedBackground("image.jpg",
                                             FramingCoordinates(5, 10, 15, 20));

  EXPECT_FALSE(service_->GetCurrentColorTheme());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // After re-enabling the kNTPCustomBackgroundEnabledByPolicy, the policy theme
  // color should be active.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, true);
  EXPECT_TRUE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  ASSERT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(managed_color, service_->GetCurrentColorTheme()->color());
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
  EXPECT_TRUE(service_->GetRecentlyUsedBackgrounds().empty());

  // After removing the policy theme color, the originally set color should be
  // active.
  pref_service_->RemoveManagedPref(themes::prefs::kPolicyThemeColor);
  EXPECT_FALSE(service_->IsCustomizationDisabledOrColorManagedByPolicy());
  EXPECT_EQ(color_theme, service_->GetCurrentColorTheme());
  EXPECT_EQ(2u, service_->GetRecentlyUsedBackgrounds().size());
}

TEST_F(HomeBackgroundCustomizationServiceTest,
       ClearsUnusedUserImagesOnInitialization) {
  // Generate test data.
  gfx::Image image1 = gfx::test::CreateImage(10, 10);
  base::FilePath image1_file_path =
      user_image_manager_->StoreUserUploadedImage(image1.ToUIImage());
  gfx::Image image2 = gfx::test::CreateImage(20, 20);
  base::FilePath image2_file_path =
      user_image_manager_->StoreUserUploadedImage(image2.ToUIImage());

  // Set up recently used items on disk. Only image 1 will be in the recently
  // used items list, so image 2 should be deleted.
  HomeUserUploadedBackground background1;
  background1.image_path = image1_file_path.value();
  background1.framing_coordinates = FramingCoordinates(5, 10, 15, 20);

  base::Value::List recent_backgrounds_data =
      base::Value::List().Append(background1.ToDict());
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds,
                         std::move(recent_backgrounds_data));

  // Create service to initialize and load recent items. This should remove
  // image 2 from the manager.
  CreateService();

  EXPECT_NE(nil, user_image_manager_->LoadUserUploadedImage(image1_file_path));
  EXPECT_EQ(nil, user_image_manager_->LoadUserUploadedImage(image2_file_path));
}
