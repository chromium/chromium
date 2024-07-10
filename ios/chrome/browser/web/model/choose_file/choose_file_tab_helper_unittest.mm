// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

#import <memory>

#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test suite for ChooseFileTabHelper.
class ChooseFileTabHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    tab_helper_ = ChooseFileTabHelper::GetOrCreateForWebState(web_state_.get());
  }

 protected:
  raw_ptr<ChooseFileTabHelper> tab_helper_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that calling `StopChoosingFiles()` submits file selection and that
// `IsChoosingFiles()` returns false afterwards.
TEST_F(ChooseFileTabHelperTest, StopChoosingFiles) {
  EXPECT_FALSE(tab_helper_->IsChoosingFiles());

  __block bool selection_submitted = false;

  // Test that calling `StopChoosingFiles()` forwards its arguments to the
  // controller and ends file selection.
  auto controller = std::make_unique<FakeChooseFileController>(
      ChooseFileEvent(false, std::vector<std::string>{},
                      std::vector<std::string>{}, web_state_.get()));
  NSURL* file_url = [NSURL fileURLWithPath:@"/path/to/file"];
  NSArray<NSURL*>* file_urls = @[ file_url ];
  NSString* display_string = @"display_string";
  UIImage* icon_image = [[UIImage alloc] init];
  controller->SetSubmitSelectionCompletion(
      base::BindOnce(^(const FakeChooseFileController& control) {
        selection_submitted = true;
        EXPECT_NSEQ(file_urls, control.submitted_file_urls());
        EXPECT_NSEQ(display_string, control.submitted_display_string());
        EXPECT_NSEQ(icon_image, control.submitted_icon_image());
      }));
  tab_helper_->StartChoosingFiles(std::move(controller));
  EXPECT_FALSE(selection_submitted);
  EXPECT_TRUE(tab_helper_->IsChoosingFiles());
  tab_helper_->StopChoosingFiles(file_urls, display_string, icon_image);
  EXPECT_TRUE(selection_submitted);
  EXPECT_FALSE(tab_helper_->IsChoosingFiles());

  // Reset `selection_submitted`.
  selection_submitted = false;

  // Test that calling `StopChoosingFiles()` with no arguments forwards an empty
  // list of files to the controller and ends file selection.
  controller = std::make_unique<FakeChooseFileController>(
      ChooseFileEvent(false, std::vector<std::string>{},
                      std::vector<std::string>{}, web_state_.get()));
  controller->SetSubmitSelectionCompletion(
      base::BindOnce(^(const FakeChooseFileController& control) {
        selection_submitted = true;
        EXPECT_NSEQ(@[], control.submitted_file_urls());
        EXPECT_NSEQ(nil, control.submitted_display_string());
        EXPECT_NSEQ(nil, control.submitted_icon_image());
      }));
  tab_helper_->StartChoosingFiles(std::move(controller));
  EXPECT_FALSE(selection_submitted);
  EXPECT_TRUE(tab_helper_->IsChoosingFiles());
  tab_helper_->StopChoosingFiles();
  EXPECT_TRUE(selection_submitted);
  EXPECT_FALSE(tab_helper_->IsChoosingFiles());
}

// Tests that finishing navigation to a different document stops file selection.
TEST_F(ChooseFileTabHelperTest, DidFinishNavigation) {
  EXPECT_FALSE(tab_helper_->IsChoosingFiles());

  auto controller = std::make_unique<FakeChooseFileController>(
      ChooseFileEvent(false, std::vector<std::string>{},
                      std::vector<std::string>{}, web_state_.get()));
  tab_helper_->StartChoosingFiles(std::move(controller));
  EXPECT_TRUE(tab_helper_->IsChoosingFiles());

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();

  navigation_context->SetIsSameDocument(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_TRUE(tab_helper_->IsChoosingFiles());

  navigation_context->SetIsSameDocument(false);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_FALSE(tab_helper_->IsChoosingFiles());
}

// Tests that `SetIsPresentingFilePicker()` and `IsPresentingFilePicker()` calls
// are forwarded to the controller.
TEST_F(ChooseFileTabHelperTest, SetIsPresentingFilePicker) {
  auto controller = std::make_unique<FakeChooseFileController>(
      ChooseFileEvent(false, std::vector<std::string>{},
                      std::vector<std::string>{}, web_state_.get()));
  ChooseFileController* controller_ptr = controller.get();
  tab_helper_->StartChoosingFiles(std::move(controller));
  ASSERT_TRUE(tab_helper_->IsChoosingFiles());

  tab_helper_->SetIsPresentingFilePicker(false);
  EXPECT_FALSE(tab_helper_->IsPresentingFilePicker());
  EXPECT_FALSE(controller_ptr->IsPresentingFilePicker());

  tab_helper_->SetIsPresentingFilePicker(true);
  EXPECT_TRUE(tab_helper_->IsPresentingFilePicker());
  EXPECT_TRUE(controller_ptr->IsPresentingFilePicker());
}

// Tests that `GetChooseFileEvent()` returns the event passed to the controller
// at construction.
TEST_F(ChooseFileTabHelperTest, GetChooseFileEvent) {
  ChooseFileEvent event(false, std::vector<std::string>{},
                        std::vector<std::string>{}, web_state_.get());
  auto controller = std::make_unique<FakeChooseFileController>(event);
  tab_helper_->StartChoosingFiles(std::move(controller));
  const ChooseFileEvent tab_helper_event = tab_helper_->GetChooseFileEvent();
  EXPECT_EQ(event.allow_multiple_files, tab_helper_event.allow_multiple_files);
  EXPECT_EQ(event.accept_file_extensions,
            tab_helper_event.accept_file_extensions);
  EXPECT_EQ(event.accept_mime_types, tab_helper_event.accept_mime_types);
  EXPECT_EQ(event.web_state.get(), tab_helper_event.web_state.get());
  EXPECT_EQ(event.time, tab_helper_event.time);
}
