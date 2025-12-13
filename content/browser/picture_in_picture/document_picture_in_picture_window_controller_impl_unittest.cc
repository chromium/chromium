// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_window_controller_impl.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockPipScreenCaptureCoordinator : public PipScreenCaptureCoordinator {
 public:
  explicit MockPipScreenCaptureCoordinator(WebContents* web_contents)
      : PipScreenCaptureCoordinator(web_contents) {}
  ~MockPipScreenCaptureCoordinator() override = default;

  MOCK_METHOD(void, OnPipShown, (WebContents & pip_web_contents), (override));
  MOCK_METHOD(void, OnPipClosed, (), (override));
};

class DocumentPictureInPictureWindowControllerImplTest
    : public RenderViewHostTestHarness {
 public:
  DocumentPictureInPictureWindowControllerImplTest() = default;
  ~DocumentPictureInPictureWindowControllerImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auto mock_pip_screen_capture_coordinator =
        std::make_unique<testing::NiceMock<MockPipScreenCaptureCoordinator>>(
            web_contents());
    mock_pip_screen_capture_coordinator_ =
        mock_pip_screen_capture_coordinator.get();
    web_contents()->SetUserData(PipScreenCaptureCoordinator::UserDataKey(),
                                std::move(mock_pip_screen_capture_coordinator));

    controller_ =
        DocumentPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
            web_contents());
  }

  void TearDown() override {
    // `controller_` and `mock_pip_screen_capture_coordinator_` are
    // WebContentsUserData, so they're owned by `web_contents()` and
    // will be cleaned up automatically. We don't need to delete them.
    // Null out the raw_ptr members to avoid dangling pointer errors.
    controller_ = nullptr;
    mock_pip_screen_capture_coordinator_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  raw_ptr<MockPipScreenCaptureCoordinator> mock_pip_screen_capture_coordinator_;
  raw_ptr<DocumentPictureInPictureWindowControllerImpl> controller_;
};

TEST_F(DocumentPictureInPictureWindowControllerImplTest,
       OnVisibilityChangedNotifiesPipScreenCaptureCoordinator) {
  std::unique_ptr<TestWebContents> child_web_contents =
      TestWebContents::Create(browser_context(), nullptr);
  controller_->SetChildWebContents(child_web_contents.get());

  EXPECT_CALL(*mock_pip_screen_capture_coordinator_,
              OnPipShown(testing::Ref(*child_web_contents)));
  controller_->Show();
  child_web_contents->WasShown();
}

TEST_F(DocumentPictureInPictureWindowControllerImplTest,
       OnPipClosedNotifiesPipScreenCaptureCoordinator) {
  std::unique_ptr<TestWebContents> child_web_contents =
      TestWebContents::Create(browser_context(), nullptr);
  controller_->SetChildWebContents(child_web_contents.get());
  controller_->Show();
  child_web_contents->WasShown();

  EXPECT_CALL(*mock_pip_screen_capture_coordinator_, OnPipClosed());
  controller_->Close(false);
}

}  // namespace content
