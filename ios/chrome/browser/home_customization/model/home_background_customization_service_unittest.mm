// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import <string_view>

#import "base/scoped_observation.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync/base/features.h"
#import "components/sync/protocol/entity_specifics.pb.h"
#import "components/sync/protocol/theme_specifics.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "components/sync/test/fake_sync_change_processor.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"
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

  std::string EncodeThemeIosSpecifics(
      sync_pb::ThemeIosSpecifics theme_ios_specifics) {
    std::string serialized = theme_ios_specifics.SerializeAsString();
    // Encode bytestring so it can be stored in a pref.
    return base::Base64Encode(serialized);
  }

  sync_pb::ThemeIosSpecifics DecodeThemeIosSpecifics(std::string_view encoded) {
    // This pref is base64 encoded, so decode it first.
    std::string serialized;
    base::Base64Decode(encoded, &serialized);
    sync_pb::ThemeIosSpecifics theme_ios_specifics;
    theme_ios_specifics.ParseFromString(serialized);
    return theme_ios_specifics;
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
  // Helper to cast and return the service.
  ThemeSyncableServiceIOS* theme_sync_service() {
    return static_cast<ThemeSyncableServiceIOS*>(
        service_->GetThemeSyncableService());
  }

  // Helper to begin syncing.
  syncer::FakeSyncChangeProcessor* StartSyncing() {
    EXPECT_TRUE(theme_sync_service()) << "ThemeSyncableServiceIOS is nullptr";
    auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
    syncer::FakeSyncChangeProcessor* processor_ptr = processor.get();

    theme_sync_service()->MergeDataAndStartSyncing(
        syncer::THEMES_IOS, syncer::SyncDataList(), std::move(processor));
    EXPECT_TRUE(theme_sync_service()->IsSyncing());

    return processor_ptr;
  }

  // Helper to fetch and decode a theme from a given pref.
  sync_pb::ThemeIosSpecifics GetThemeFromPref(std::string_view pref_name) {
    return DecodeThemeIosSpecifics(pref_service_->GetString(pref_name));
  }

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

  const base::ListValue& recently_used_backgrounds =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_EQ(GetDefaultRecentlyUsedImages().size(),
            recently_used_backgrounds.size());

  for (size_t i = 0; i < GetDefaultRecentlyUsedImages().size(); i++) {
    CollectionImage expected = GetDefaultRecentlyUsedImages()[i];
    const base::Value& actual = recently_used_backgrounds[i];

    EXPECT_TRUE(actual.is_string());

    sync_pb::ThemeIosSpecifics theme_specifics =
        DecodeThemeIosSpecifics(actual.GetString());

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

  const base::ListValue& recently_used_backgrounds =
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
  sync_pb::ThemeIosSpecifics disk_theme_specifics = DecodeThemeIosSpecifics(
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
  const base::ListValue& recent_backgrounds_disk =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_GE(recent_backgrounds_disk.size(), 1u);

  const base::Value& value = recent_backgrounds_disk[0];
  ASSERT_TRUE(value.is_string());
  sync_pb::ThemeIosSpecifics recent_theme_specifics_disk =
      DecodeThemeIosSpecifics(value.GetString());

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
  sync_pb::ThemeIosSpecifics disk_theme_specifics = DecodeThemeIosSpecifics(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  EXPECT_EQ(color_theme, disk_theme_specifics.user_color_theme());

  EXPECT_TRUE(
      pref_service_->GetDict(prefs::kIosUserUploadedBackground).empty());

  // Make sure recent backgrounds disk data has this item first.
  const base::ListValue& recent_backgrounds_disk =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);

  ASSERT_GE(recent_backgrounds_disk.size(), 1u);

  const base::Value& value = recent_backgrounds_disk[0];
  ASSERT_TRUE(value.is_string());
  sync_pb::ThemeIosSpecifics recent_theme_specifics_disk =
      DecodeThemeIosSpecifics(value.GetString());

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
  const base::ListValue& recent_backgrounds_disk =
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
  const base::ListValue& initial_recent_backgrounds =
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

  sync_pb::ThemeIosSpecifics color_theme_specifics;
  *color_theme_specifics.mutable_user_color_theme() = color_theme;

  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();

  base::ListValue recent_backgrounds_data =
      base::ListValue()
          .Append(EncodeThemeIosSpecifics(color_theme_specifics))
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

  base::ListValue recent_backgrounds_data =
      base::ListValue().Append(background1.ToDict());
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds,
                         std::move(recent_backgrounds_data));

  // Create service to initialize and load recent items. This should remove
  // image 2 from the manager.
  CreateService();

  EXPECT_NE(nil, user_image_manager_->LoadUserUploadedImage(image1_file_path));
  EXPECT_EQ(nil, user_image_manager_->LoadUserUploadedImage(image2_file_path));
}

// Tests that when `syncer::kSyncThemesIos` is enabled, the service migrates
// legacy theme data to the new theme pref and sets the migration flag.
TEST_F(HomeBackgroundCustomizationServiceTest,
       MigratesLegacyThemeWhenSyncEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});

  sync_pb::UserColorTheme expected_theme = GenerateUserColorTheme(0xff0000);

  sync_pb::ThemeIosSpecifics legacy_data;
  *legacy_data.mutable_user_color_theme() = expected_theme;

  pref_service_->SetString(prefs::kIosSavedThemeSpecificsIos,
                           EncodeThemeIosSpecifics(legacy_data));

  // Ensure new pref and migration flag are empty/false.
  pref_service_->SetString(prefs::kIosNtpThemeSpecifics, "");
  pref_service_->SetBoolean(prefs::kIosNtpThemeMigrationComplete, false);

  CreateService();

  // Verify migration flag is set.
  EXPECT_TRUE(pref_service_->GetBoolean(prefs::kIosNtpThemeMigrationComplete));

  // Verify migration occurred.
  std::string current_theme =
      pref_service_->GetString(prefs::kIosNtpThemeSpecifics);
  EXPECT_FALSE(current_theme.empty());

  sync_pb::ThemeIosSpecifics migrated_theme =
      DecodeThemeIosSpecifics(current_theme);

  EXPECT_EQ(expected_theme, migrated_theme.user_color_theme());
  ASSERT_TRUE(service_->GetCurrentColorTheme());
  EXPECT_EQ(expected_theme, service_->GetCurrentColorTheme());
}

// Tests that if the user has already migrated, cleared their background (so new
// theme pref is empty), and restarts, the legacy theme is NOT resurrected.
TEST_F(HomeBackgroundCustomizationServiceTest,
       DoesNotResurrectLegacyThemeAfterClear) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});

  // User has a legacy theme, but migration is marked complete (this simulates a
  // user who migrated, then cleared their background.)
  sync_pb::UserColorTheme legacy_theme = GenerateUserColorTheme(0xff0000);
  sync_pb::ThemeIosSpecifics legacy_data;
  *legacy_data.mutable_user_color_theme() = legacy_theme;

  pref_service_->SetString(prefs::kIosSavedThemeSpecificsIos,
                           EncodeThemeIosSpecifics(legacy_data));

  pref_service_->SetString(prefs::kIosNtpThemeSpecifics, "");
  pref_service_->SetBoolean(prefs::kIosNtpThemeMigrationComplete, true);

  CreateService();

  std::string current_theme =
      pref_service_->GetString(prefs::kIosNtpThemeSpecifics);
  EXPECT_TRUE(current_theme.empty());
  EXPECT_FALSE(service_->GetCurrentColorTheme().has_value());
}

// Tests that when `syncer::kSyncThemesIos` is enabled AND the user is NOT
// actively syncing, theme data is saved to both the new pref and the legacy
// pref.
TEST_F(HomeBackgroundCustomizationServiceTest,
       DualWritesToBothPrefsWhenSyncFeatureEnabledButNotSyncing) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});

  CreateService();

  // Verify sync is not running.
  ASSERT_TRUE(theme_sync_service());
  ASSERT_FALSE(theme_sync_service()->IsSyncing());

  // Set a theme locally.
  sync_pb::UserColorTheme expected_theme = GenerateUserColorTheme(0x00ff00);
  service_->SetBackgroundColor(expected_theme.color(),
                               expected_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  // Verify both prefs were written to (dual writes).
  EXPECT_FALSE(pref_service_->GetString(prefs::kIosNtpThemeSpecifics).empty());
  EXPECT_EQ(expected_theme,
            GetThemeFromPref(prefs::kIosNtpThemeSpecifics).user_color_theme());

  EXPECT_FALSE(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos).empty());
  EXPECT_EQ(
      expected_theme,
      GetThemeFromPref(prefs::kIosSavedThemeSpecificsIos).user_color_theme());
}

// Tests that when `syncer::kSyncThemesIos` is enabled AND the user IS actively
// syncing, theme data is only saved to the new theme pref. The legacy pref acts
// as a snapshot before signing in.
TEST_F(HomeBackgroundCustomizationServiceTest,
       DoesNotOverwriteSnapshotWhenActivelySyncing) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});

  CreateService();

  // Establish a pre-sync snapshot exactly like a real user would.
  sync_pb::UserColorTheme snapshot_theme = GenerateUserColorTheme(0xff0000);
  service_->SetBackgroundColor(snapshot_theme.color(),
                               snapshot_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  StartSyncing();

  // Make a local theme change while syncing.
  sync_pb::UserColorTheme active_theme = GenerateUserColorTheme(0x00ff00);
  service_->SetBackgroundColor(active_theme.color(),
                               active_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  // Verify the new pref updated to the new theme.
  EXPECT_FALSE(pref_service_->GetString(prefs::kIosNtpThemeSpecifics).empty());
  EXPECT_EQ(active_theme,
            GetThemeFromPref(prefs::kIosNtpThemeSpecifics).user_color_theme());

  // Verify the snapshot pref was NOT updated (remains the initial snapshot).
  EXPECT_EQ(
      snapshot_theme,
      GetThemeFromPref(prefs::kIosSavedThemeSpecificsIos).user_color_theme());
}

// Tests that clearing the background clears both prefs when sync is not active.
TEST_F(HomeBackgroundCustomizationServiceTest, ClearsBothPrefsWhenNotSyncing) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});

  CreateService();

  // Set a background first
  sync_pb::UserColorTheme theme = GenerateUserColorTheme(0x00ff00);
  service_->SetBackgroundColor(theme.color(), theme.browser_color_variant());
  service_->StoreCurrentTheme();

  EXPECT_FALSE(pref_service_->GetString(prefs::kIosNtpThemeSpecifics).empty());
  EXPECT_FALSE(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos).empty());

  // Clear the theme.
  service_->ClearCurrentBackground();
  service_->StoreCurrentTheme();

  // Verify both theme prefs are cleared.
  EXPECT_TRUE(pref_service_->GetString(prefs::kIosNtpThemeSpecifics).empty());
  EXPECT_TRUE(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos).empty());
}

// Tests that `GetCurrentTheme` returns the currently active theme specifics.
TEST_F(HomeBackgroundCustomizationServiceTest, DelegateGetCurrentTheme) {
  CreateService();

  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff00ff);
  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());

  sync_pb::ThemeIosSpecifics local_theme = service_->GetCurrentTheme();

  EXPECT_TRUE(local_theme.has_user_color_theme());
  EXPECT_EQ(color_theme, local_theme.user_color_theme());
}

// Tests that `ApplyTheme()` from the sync delegate correctly updates the local
// theme, clears any user-uploaded images, and alerts UI observers.
TEST_F(HomeBackgroundCustomizationServiceTest, DelegateApplyTheme) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});
  CreateService();

  // Simulate a user having an uploaded image initially.
  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();
  service_->SetCurrentUserUploadedBackground(
      user_background.image_path, user_background.framing_coordinates);
  EXPECT_TRUE(service_->GetCurrentCustomBackground());

  sync_pb::ThemeIosSpecifics specifics;
  *specifics.mutable_user_color_theme() = GenerateUserColorTheme(0xff00ff);

  observer_.on_background_changed_called = false;

  // Simulate Sync pushing a remote theme down to the delegate.
  service_->ApplyTheme(specifics);

  EXPECT_TRUE(observer_.on_background_changed_called);
  ASSERT_TRUE(service_->GetCurrentColorTheme().has_value());
  EXPECT_EQ(specifics.user_color_theme(),
            service_->GetCurrentColorTheme().value());
  EXPECT_EQ(specifics, service_->GetCurrentTheme());

  // Verify the user uploaded background was cleared.
  EXPECT_FALSE(service_->GetCurrentCustomBackground());
}

// Tests that `CacheLocalTheme()` and `RestoreCachedTheme()` successfully
// snapshot and revert the local UI state.
TEST_F(HomeBackgroundCustomizationServiceTest, DelegateCacheAndRestoreTheme) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});
  CreateService();

  // Set initial theme (take snapshot).
  sync_pb::UserColorTheme initial_theme = GenerateUserColorTheme(0x111111);
  service_->SetBackgroundColor(initial_theme.color(),
                               initial_theme.browser_color_variant());
  service_->StoreCurrentTheme();

  // Simulate starting sync (which triggers a cache).
  theme_sync_service()->WillStartInitialSync();
  StartSyncing();

  // Verify it was written to the legacy/snapshot pref.
  EXPECT_EQ(
      initial_theme,
      GetThemeFromPref(prefs::kIosSavedThemeSpecificsIos).user_color_theme());

  // Change the theme locally while syncing.
  sync_pb::UserColorTheme new_theme = GenerateUserColorTheme(0x222222);
  service_->SetBackgroundColor(new_theme.color(),
                               new_theme.browser_color_variant());
  service_->StoreCurrentTheme();
  EXPECT_EQ(new_theme, service_->GetCurrentColorTheme().value());

  observer_.on_background_changed_called = false;

  // Simulate stopping sync (which triggers a restore)
  theme_sync_service()->StopSyncing(syncer::THEMES_IOS);

  // Expect a revert back to initial theme.
  EXPECT_TRUE(observer_.on_background_changed_called);
  ASSERT_TRUE(service_->GetCurrentColorTheme().has_value());
  EXPECT_EQ(initial_theme, service_->GetCurrentColorTheme().value());
}

// Tests that `IsCurrentThemeSyncable()` prevents uploading custom user images.
TEST_F(HomeBackgroundCustomizationServiceTest, DelegateIsThemeSyncable) {
  CreateService();

  // Default state is syncable.
  EXPECT_TRUE(service_->IsCurrentThemeSyncable());

  // Color themes are syncable.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xff0000);
  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());
  EXPECT_TRUE(service_->IsCurrentThemeSyncable());

  // User uploaded images are local-only and NOT syncable.
  HomeUserUploadedBackground user_background =
      GenerateHomeUserUploadedBackground();
  service_->SetCurrentUserUploadedBackground(
      user_background.image_path, user_background.framing_coordinates);

  EXPECT_FALSE(service_->IsCurrentThemeSyncable());

  // Clear user image. Should be syncable again.
  service_->ClearCurrentUserUploadedBackground();
  EXPECT_TRUE(service_->IsCurrentThemeSyncable());

  // Policy themes are NOT syncable.
  pref_service_->SetManagedPref(themes::prefs::kPolicyThemeColor,
                                base::Value(static_cast<int>(0x0000ff)));
  EXPECT_FALSE(service_->IsCurrentThemeSyncable());
}

// Tests that `IsCurrentThemeManagedByPolicy()` correctly identifies managed
// state.
TEST_F(HomeBackgroundCustomizationServiceTest,
       DelegateIsCurrentThemeManagedByPolicy) {
  CreateService();

  // By default, not managed.
  EXPECT_FALSE(service_->IsCurrentThemeManagedByPolicy());

  // Managed by disable customization policy.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, false);
  EXPECT_TRUE(service_->IsCurrentThemeManagedByPolicy());

  // Reset policy.
  pref_service_->SetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy, true);
  EXPECT_FALSE(service_->IsCurrentThemeManagedByPolicy());

  // Managed by theme color policy.
  pref_service_->SetManagedPref(themes::prefs::kPolicyThemeColor,
                                base::Value(static_cast<int>(0x0000ff)));
  EXPECT_TRUE(service_->IsCurrentThemeManagedByPolicy());
}

// Tests that changing the theme locally automatically propagates the update
// to the sync service.
TEST_F(HomeBackgroundCustomizationServiceTest, LocalChangesTriggerSync) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {kNTPBackgroundCustomization, syncer::kSyncThemesIos}, {});
  CreateService();

  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  ASSERT_TRUE(processor);
  EXPECT_EQ(0u, processor->changes().size());

  // Make a local change.
  sync_pb::UserColorTheme color_theme = GenerateUserColorTheme(0xabcdef);
  service_->SetBackgroundColor(color_theme.color(),
                               color_theme.browser_color_variant());

  // Verify that the change was successfully sent out to Sync.
  const syncer::SyncChangeList& changes = processor->changes();
  ASSERT_EQ(1u, changes.size());

  // Local changes should always be sent as an UPDATE action.
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());

  EXPECT_TRUE(changes[0].sync_data().GetSpecifics().has_theme_ios());
  EXPECT_EQ(
      color_theme,
      changes[0].sync_data().GetSpecifics().theme_ios().user_color_theme());
}
