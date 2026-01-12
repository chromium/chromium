// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/ntp_background_image_cache_service.h"

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/themes/ntp_background_service.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_unittest_util.h"

// Tests to verify the NTPBackgroundImageCacheService.
class NTPBackgroundImageCacheServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kNTPBackgroundCustomization);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    HomeBackgroundCustomizationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kNTPCustomBackgroundEnabledByPolicy, true);
    pref_service_->registry()->RegisterIntegerPref(
        themes::prefs::kPolicyThemeColor, SK_ColorTRANSPARENT);

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
            background_image_service_.get());

    cache_service_ = std::make_unique<NTPBackgroundImageCacheService>(
        background_customization_service_.get());
  }

  void TearDown() override {
    ntp_background_service_->Shutdown();
    ntp_background_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  std::unique_ptr<NtpBackgroundService> ntp_background_service_;
  std::unique_ptr<FakeUserUploadedImageManager> user_image_manager_;
  std::unique_ptr<FakeHomeBackgroundImageService> background_image_service_;
  std::unique_ptr<HomeBackgroundCustomizationService>
      background_customization_service_;
  std::unique_ptr<NTPBackgroundImageCacheService> cache_service_;
};

// Tests setting and getting the cached image.
TEST_F(NTPBackgroundImageCacheServiceTest, SetAndGetImage) {
  UIImage* image = gfx::test::CreateImage(10, 10).ToUIImage();
  cache_service_->SetCachedBackgroundImage(image);
  EXPECT_EQ(image, cache_service_->GetCachedBackgroundImage());
}

// Tests that the cache is cleared when the background changes.
TEST_F(NTPBackgroundImageCacheServiceTest, ClearsOnBackgroundChange) {
  UIImage* image = gfx::test::CreateImage(10, 10).ToUIImage();
  cache_service_->SetCachedBackgroundImage(image);
  EXPECT_EQ(image, cache_service_->GetCachedBackgroundImage());

  // Simulate background change.
  background_customization_service_->SetBackgroundColor(
      SK_ColorRED, sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT);

  EXPECT_EQ(nil, cache_service_->GetCachedBackgroundImage());
}
