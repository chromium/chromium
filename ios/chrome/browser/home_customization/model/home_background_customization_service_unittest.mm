// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import "base/files/scoped_temp_dir.h"
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
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
