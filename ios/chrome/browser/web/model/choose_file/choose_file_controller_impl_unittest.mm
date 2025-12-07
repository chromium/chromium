// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_impl.h"

#import "base/functional/callback_helpers.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test suite for ChooseFileControllerImpl.
class ChooseFileControllerImplTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that `SetIsPresentingFilePicker()` and `IsPresentingFilePicker()` work
// as expected.
TEST_F(ChooseFileControllerImplTest, IsPresentingFilePicker) {
  auto controller = std::make_unique<ChooseFileControllerImpl>(
      ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build(),
      base::DoNothing());

  EXPECT_FALSE(controller->IsPresentingFilePicker());
  controller->SetIsPresentingFilePicker(true);
  EXPECT_TRUE(controller->IsPresentingFilePicker());
  controller->SetIsPresentingFilePicker(false);
  EXPECT_FALSE(controller->IsPresentingFilePicker());
}

// Tests that `DoSubmitSelection()` calls the completion handler and that
// `HasExpired()` returns true afterwards.
TEST_F(ChooseFileControllerImplTest, DoSubmitSelection) {
  __block bool completion_called = false;
  NSURL* file_url = [NSURL fileURLWithPath:@"/path/to/file"];
  NSArray<NSURL*>* file_urls = @[ file_url ];

  auto controller = std::make_unique<ChooseFileControllerImpl>(
      ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build(),
      base::BindOnce(^(NSArray<NSURL*>* result_urls) {
        completion_called = true;
        EXPECT_NSEQ(file_urls, result_urls);
      }));

  EXPECT_FALSE(controller->HasExpired());
  EXPECT_FALSE(completion_called);

  controller->DoSubmitSelection(file_urls, nil, nil);

  EXPECT_TRUE(completion_called);
  EXPECT_TRUE(controller->HasExpired());
}

// Tests that destroying the controller without submitting a selection calls the
// completion handler with no list of files (nil).
TEST_F(ChooseFileControllerImplTest, DestroyWithoutSubmitting) {
  __block bool completion_called = false;

  auto controller = std::make_unique<ChooseFileControllerImpl>(
      ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build(),
      base::BindOnce(^(NSArray<NSURL*>* result_urls) {
        completion_called = true;
        EXPECT_NSEQ(nil, result_urls);
      }));

  EXPECT_FALSE(completion_called);

  controller->SetIsPresentingFilePicker(true);
  controller.reset();

  EXPECT_TRUE(completion_called);
}
