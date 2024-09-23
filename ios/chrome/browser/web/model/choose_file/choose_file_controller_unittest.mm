// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

#import <memory>

#import "base/test/gtest_util.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test suite for ChooseFileController.
class ChooseFileControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    event_ = std::make_unique<ChooseFileEvent>(
        false, std::vector<std::string>{}, std::vector<std::string>{},
        web_state_.get());
    controller_ = std::make_unique<FakeChooseFileController>(*event_);
  }

 protected:
  std::unique_ptr<ChooseFileEvent> event_;
  std::unique_ptr<FakeChooseFileController> controller_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that calling `SubmitSelection()` forwards its input to
// `DoSubmitSelection()` if the controller has not expired, and then is no
// longer waiting for selection.
TEST_F(ChooseFileControllerTest, SubmitSelection) {
  EXPECT_FALSE(controller_->HasSubmittedSelection());

  NSURL* file_url = [NSURL fileURLWithPath:@"/path/to/file"];
  NSArray<NSURL*>* file_urls = @[ file_url ];
  NSString* display_string = @"display_string";
  UIImage* icon_image = [[UIImage alloc] init];
  controller_->SubmitSelection(file_urls, display_string, icon_image);

  EXPECT_EQ(file_urls, controller_->submitted_file_urls());
  EXPECT_EQ(display_string, controller_->submitted_display_string());
  EXPECT_EQ(icon_image, controller_->submitted_icon_image());

  EXPECT_TRUE(controller_->HasSubmittedSelection());
}

// Tests that calling `SubmitSelection()` does NOT forwards its input to
// `DoSubmitSelection()` if the controller has expired, and then is no longer
// waiting for selection.
TEST_F(ChooseFileControllerTest, SubmitSelectionExpired) {
  EXPECT_FALSE(controller_->HasSubmittedSelection());

  NSURL* file_url = [NSURL fileURLWithPath:@"/path/to/file"];
  NSArray<NSURL*>* file_urls = @[ file_url ];
  NSString* display_string = @"display_string";
  UIImage* icon_image = [[UIImage alloc] init];
  controller_->SetHasExpired(true);
  controller_->SubmitSelection(file_urls, display_string, icon_image);

  EXPECT_EQ(nil, controller_->submitted_file_urls());
  EXPECT_EQ(nil, controller_->submitted_display_string());
  EXPECT_EQ(nil, controller_->submitted_icon_image());

  EXPECT_TRUE(controller_->HasSubmittedSelection());
}

// Tests that `GetChooseFileEvent()` returns the event passed to the controller
// at construction.
TEST_F(ChooseFileControllerTest, GetChooseFileEvent) {
  const ChooseFileEvent event = controller_->GetChooseFileEvent();
  EXPECT_EQ(event_->allow_multiple_files, event.allow_multiple_files);
  EXPECT_EQ(event_->accept_file_extensions, event.accept_file_extensions);
  EXPECT_EQ(event_->accept_mime_types, event.accept_mime_types);
  EXPECT_EQ(event_->web_state.get(), event.web_state.get());
  EXPECT_EQ(event_->time, event.time);
}
