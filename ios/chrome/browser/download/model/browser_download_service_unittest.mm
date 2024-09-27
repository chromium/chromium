// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service.h"

#import <vector>

#import "base/memory/ptr_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
const char kUrl[] = "https://test.test/";
const base::FilePath::CharType kUsdzFileName[] =
    FILE_PATH_LITERAL("important_file.usdz");
const base::FilePath::CharType kRealityFileName[] =
    FILE_PATH_LITERAL("important_file.reality");

// Substitutes real TabHelper for testing.
template <class TabHelper>
class StubTabHelper : public TabHelper {
 public:
  // Overrides the method from web::WebStateUserData<TabHelper>.
  template <typename... Args>
  static void CreateForWebState(web::WebState* web_state, Args&&... args) {
    web_state->SetUserData(TabHelper::UserDataKey(),
                           base::WrapUnique(new StubTabHelper(
                               web_state, std::forward<Args>(args)...)));
  }

  // Adds the given task to tasks() lists.
  void Download(std::unique_ptr<web::DownloadTask> task) override {
    tasks_.push_back(std::move(task));
  }

  // Tasks added via Download() call.
  using DownloadTasks = std::vector<std::unique_ptr<web::DownloadTask>>;
  const DownloadTasks& tasks() const { return tasks_; }

  StubTabHelper(web::WebState* web_state) : TabHelper(web_state) {}

 private:
  DownloadTasks tasks_;
};

// Substitutes DownloadManagerTabHelper for testing. This is necessary since the
// method used to add a task to the tab helper has a different name.
template <>
class StubTabHelper<DownloadManagerTabHelper>
    : public DownloadManagerTabHelper {
 public:
  // Overrides the method from web::WebStateUserData<TabHelper>.
  template <typename... Args>
  static void CreateForWebState(web::WebState* web_state, Args&&... args) {
    web_state->SetUserData(DownloadManagerTabHelper::UserDataKey(),
                           base::WrapUnique(new StubTabHelper(
                               web_state, std::forward<Args>(args)...)));
  }

  // Adds the given task to tasks() lists.
  void SetCurrentDownload(std::unique_ptr<web::DownloadTask> task) override {
    tasks_.push_back(std::move(task));
  }

  // Tasks added via Download() call.
  using DownloadTasks = std::vector<std::unique_ptr<web::DownloadTask>>;
  const DownloadTasks& tasks() const { return tasks_; }

  StubTabHelper(web::WebState* web_state)
      : DownloadManagerTabHelper(web_state) {}

 private:
  DownloadTasks tasks_;
};

}  // namespace

// Test fixture for testing BrowserDownloadService class.
class BrowserDownloadServiceTest : public PlatformTest {
 protected:
  BrowserDownloadServiceTest() : profile_(TestProfileIOS::Builder().Build()) {
    StubTabHelper<PassKitTabHelper>::CreateForWebState(&web_state_);
    StubTabHelper<ARQuickLookTabHelper>::CreateForWebState(&web_state_);
    StubTabHelper<VcardTabHelper>::CreateForWebState(&web_state_);
    StubTabHelper<DownloadManagerTabHelper>::CreateForWebState(&web_state_);
    web_state_.SetBrowserState(profile_.get());
  }

  web::DownloadController* download_controller() {
    return web::DownloadController::FromBrowserState(profile_.get());
  }

  StubTabHelper<PassKitTabHelper>* pass_kit_tab_helper() {
    return static_cast<StubTabHelper<PassKitTabHelper>*>(
        PassKitTabHelper::GetOrCreateForWebState(&web_state_));
  }

  StubTabHelper<ARQuickLookTabHelper>* ar_quick_look_tab_helper() {
    return static_cast<StubTabHelper<ARQuickLookTabHelper>*>(
        ARQuickLookTabHelper::GetOrCreateForWebState(&web_state_));
  }

  StubTabHelper<VcardTabHelper>* vcard_tab_helper() {
    return static_cast<StubTabHelper<VcardTabHelper>*>(
        VcardTabHelper::FromWebState(&web_state_));
  }

  StubTabHelper<DownloadManagerTabHelper>* download_manager_tab_helper() {
    return static_cast<StubTabHelper<DownloadManagerTabHelper>*>(
        DownloadManagerTabHelper::FromWebState(&web_state_));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  base::HistogramTester histogram_tester_;
};

// Tests that BrowserDownloadService downloads the task using
// PassKitTabHelper.
TEST_F(BrowserDownloadServiceTest, PkPassMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, pass_kit_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, pass_kit_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(DownloadMimeTypeResult::PkPass),
      1);
}

// Tests that BrowserDownloadService uses ARQuickLookTabHelper for .USDZ
// extension.
TEST_F(BrowserDownloadServiceTest, UsdzExtension) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "other");
  task->SetGeneratedFileName(base::FilePath(kUsdzFileName));
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, ar_quick_look_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, ar_quick_look_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(DownloadMimeTypeResult::Other),
      1);
}

// Tests that BrowserDownloadService uses ARQuickLookTabHelper for .REALITY
// extension.
TEST_F(BrowserDownloadServiceTest, RealityExtension) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "other");
  task->SetGeneratedFileName(base::FilePath(kRealityFileName));
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, ar_quick_look_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, ar_quick_look_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(DownloadMimeTypeResult::Other),
      1);
}
// Tests that BrowserDownloadService uses ARQuickLookTabHelper for USDZ Mime
// type.
TEST_F(BrowserDownloadServiceTest, UsdzMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kUsdzMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, ar_quick_look_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, ar_quick_look_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::UniversalSceneDescription),
      1);
}

// Tests that BrowserDownloadService uses ARQuickLookTabHelper for legacy USDZ
// Mime type.
TEST_F(BrowserDownloadServiceTest, LegacyUsdzMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kLegacyUsdzMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, ar_quick_look_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, ar_quick_look_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::LegacyUniversalSceneDescription),
      1);
}

// Tests that BrowserDownloadService uses ARQuickLookTabHelper for legacy Pixar
// USDZ Mime type.
TEST_F(BrowserDownloadServiceTest, LegacyPixarUsdzMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl),
                                                      kLegacyPixarUsdzMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, ar_quick_look_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, ar_quick_look_tab_helper()->tasks()[0].get());
  ASSERT_TRUE(download_manager_tab_helper()->tasks().empty());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::LegacyPixarUniversalSceneDescription),
      1);
}

// Tests that BrowserDownloadService uses DownloadManagerTabHelper for PDF Mime
// Type.
TEST_F(BrowserDownloadServiceTest, PdfMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL(kUrl), kAdobePortableDocumentFormatMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_TRUE(pass_kit_tab_helper()->tasks().empty());
  ASSERT_EQ(1U, download_manager_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, download_manager_tab_helper()->tasks()[0].get());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::AdobePortableDocumentFormat),
      1);
}

// Tests that BrowserDownloadService uses DownloadManagerTabHelper for Zip Mime
// Type.
TEST_F(BrowserDownloadServiceTest, ZipArchiveMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kZipArchiveMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_TRUE(pass_kit_tab_helper()->tasks().empty());
  ASSERT_EQ(1U, download_manager_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, download_manager_tab_helper()->tasks()[0].get());
  histogram_tester_.ExpectUniqueSample("Download.IOSDownloadMimeType",
                                       static_cast<base::HistogramBase::Sample>(
                                           DownloadMimeTypeResult::ZipArchive),
                                       1);
}

// Tests that BrowserDownloadService uses DownloadManagerTabHelper for .exe Mime
// Type.
TEST_F(BrowserDownloadServiceTest, ExeMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL(kUrl), kMicrosoftApplicationMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_TRUE(pass_kit_tab_helper()->tasks().empty());
  ASSERT_EQ(1U, download_manager_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, download_manager_tab_helper()->tasks()[0].get());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::MicrosoftApplication),
      1);
}

// Tests that BrowserDownloadService uses DownloadManagerTabHelper for .apk Mime
// Type.
TEST_F(BrowserDownloadServiceTest, ApkMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL(kUrl), kAndroidPackageArchiveMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_TRUE(pass_kit_tab_helper()->tasks().empty());
  ASSERT_EQ(1U, download_manager_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, download_manager_tab_helper()->tasks()[0].get());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadMimeType",
      static_cast<base::HistogramBase::Sample>(
          DownloadMimeTypeResult::AndroidPackageArchive),
      1);
}

// Tests that the code doesn't crash if the download manager tab helper hasn't
// been created for this webstate.
TEST_F(BrowserDownloadServiceTest, NoDownloadManager) {
  web::FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(profile_.get());

  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "test/test");
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &fake_web_state, std::move(task));
  ASSERT_EQ(0U, download_manager_tab_helper()->tasks().size());
}

// Tests downloading a valid vcard file while the kill switch is enabled.
TEST_F(BrowserDownloadServiceTest, VCardKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kVCardKillSwitch);

  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kVcardMimeType);
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(0U, vcard_tab_helper()->tasks().size());
}

// Tests downloading a valid AR file while the kill switch is enabled.
TEST_F(BrowserDownloadServiceTest, ARKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kARKillSwitch);

  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kUsdzMimeType);
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(0U, ar_quick_look_tab_helper()->tasks().size());
}

// Tests downloading a valid PKPass file while the kill switch is enabled.
TEST_F(BrowserDownloadServiceTest, PassKitKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPassKitKillSwitch);

  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  EXPECT_EQ(0U, pass_kit_tab_helper()->tasks().size());
}
