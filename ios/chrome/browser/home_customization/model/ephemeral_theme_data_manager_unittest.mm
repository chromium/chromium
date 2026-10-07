// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/ephemeral_theme_data_manager.h"

#import <memory>
#import <optional>
#import <string>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "net/http/http_status_code.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kAnimationUrl[] = "https://www.gstatic.com/theme_animation.json";
constexpr char kAnimationColorMappingJson[] =
    R"({"light":{"**.Theme.Fill 1.Color":"#34A853"}})";
constexpr char kSeedColor[] = "#1A73E8";
constexpr char kAnimationLottieJsonBody[] =
    R"({"v":"5.7.4","name":"theme","layers":[]})";

}  // namespace

class EphemeralThemeDataManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    EphemeralThemeDataManager::RegisterProfilePrefs(pref_service_->registry());
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    manager_ = std::make_unique<EphemeralThemeDataManager>(
        pref_service_.get(), test_shared_loader_factory_, temp_dir_.GetPath());
  }

  void InitEphemeralThemeFeatureWithValidParams() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kNewTabPageEphemeralTheme,
        {{"animation-url", kAnimationUrl},
         {"animation-colormapping", kAnimationColorMappingJson},
         {"seed-color", kSeedColor},
         {"version", "1"}});
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<EphemeralThemeDataManager> manager_;
};

// Test that `FetchEphemeralThemeData` downloads the animation assets, writes
// them to disk, populates `prefs::kIosNtpEphemeralThemeData`, and invokes the
// completion callback once all parallel operations complete.
TEST_F(EphemeralThemeDataManagerTest, FetchesAndSavesAllAssetsInParallel) {
  InitEphemeralThemeFeatureWithValidParams();

  base::test::TestFuture<void> completion_future;
  manager_->FetchEphemeralThemeData(HomeCustomizationBackgroundStyle::kColor,
                                    completion_future.GetCallback());

  EXPECT_TRUE(test_url_loader_factory_.IsPending(kAnimationUrl));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kAnimationUrl, kAnimationLottieJsonBody);

  ASSERT_TRUE(completion_future.Wait());
  EXPECT_TRUE(manager_->HasCachedData());
  EXPECT_EQ(HomeCustomizationBackgroundStyle::kColor,
            manager_->GetPreEphemeralBackgroundStyle());

  base::FilePath bundle_dir =
      temp_dir_.GetPath().AppendASCII(kEphemeralThemeDirectoryName);
  base::FilePath expected_animation_file =
      bundle_dir.AppendASCII(kEphemeralThemeAnimationFileName);

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(expected_animation_file, &file_contents));
  EXPECT_EQ(kAnimationLottieJsonBody, file_contents);

  const base::DictValue& saved_dict =
      pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData);
  const std::string* saved_seed =
      saved_dict.FindString(kEphemeralThemeSeedColorKey);
  ASSERT_TRUE(saved_seed);
  EXPECT_EQ(kSeedColor, *saved_seed);
  EXPECT_EQ(1, saved_dict.FindInt(kEphemeralThemeVersionKey));
}

// Test that if the asset download fails, `prefs::kIosNtpEphemeralThemeData` is
// not populated and the completion callback is not invoked.
TEST_F(EphemeralThemeDataManagerTest, SkipsPrefWriteWhenDownloadFails) {
  InitEphemeralThemeFeatureWithValidParams();

  bool completion_called = false;
  base::RunLoop run_loop;
  manager_->FetchEphemeralThemeData(
      HomeCustomizationBackgroundStyle::kDefault,
      base::BindOnce(
          [](bool* called, base::ScopedClosureRunner) { *called = true; },
          &completion_called,
          base::ScopedClosureRunner(run_loop.QuitClosure())));

  base::FilePath bundle_dir =
      temp_dir_.GetPath().AppendASCII(kEphemeralThemeDirectoryName);
  base::FilePath animation_file =
      bundle_dir.AppendASCII(kEphemeralThemeAnimationFileName);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kAnimationUrl, std::string(), net::HTTP_NOT_FOUND);
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(animation_file));
  EXPECT_FALSE(completion_called);
  EXPECT_FALSE(manager_->HasCachedData());
}

// Test that `UpdatePreEphemeralBackgroundStyle` updates the saved background
// style and `CleanupEphemeralThemeData` deletes the saved files (including any
// legacy promo file in the ephemeral theme directory) and clears the pref.
TEST_F(EphemeralThemeDataManagerTest,
       UpdatesBackgroundStyleAndCleansUpFilesAndPrefs) {
  InitEphemeralThemeFeatureWithValidParams();

  base::test::TestFuture<void> completion_future;
  manager_->FetchEphemeralThemeData(HomeCustomizationBackgroundStyle::kDefault,
                                    completion_future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kAnimationUrl, kAnimationLottieJsonBody);
  ASSERT_TRUE(completion_future.Wait());

  EXPECT_EQ(HomeCustomizationBackgroundStyle::kDefault,
            manager_->GetPreEphemeralBackgroundStyle());

  manager_->UpdatePreEphemeralBackgroundStyle(
      HomeCustomizationBackgroundStyle::kPreset);
  EXPECT_EQ(HomeCustomizationBackgroundStyle::kPreset,
            manager_->GetPreEphemeralBackgroundStyle());

  base::FilePath bundle_dir =
      temp_dir_.GetPath().AppendASCII(kEphemeralThemeDirectoryName);
  base::FilePath legacy_promo_file =
      bundle_dir.AppendASCII("ephemeral_promo.json");
  ASSERT_TRUE(base::WriteFile(legacy_promo_file, "{}"));

  manager_->CleanupEphemeralThemeData();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !manager_->HasCachedData() && !base::PathExists(bundle_dir);
  }));
}
