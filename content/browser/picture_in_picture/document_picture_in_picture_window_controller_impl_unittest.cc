// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_window_controller_impl.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockPipScreenCaptureCoordinator : public PipScreenCaptureCoordinator {
 public:
  MockPipScreenCaptureCoordinator() = default;
  ~MockPipScreenCaptureCoordinator() override = default;

  MOCK_METHOD(void,
              OnPipShown,
              (WebContents&, const GlobalRenderFrameHostId&),
              (override));
  MOCK_METHOD(void, OnPipClosed, (), (override));
  MOCK_METHOD(std::unique_ptr<PipScreenCaptureCoordinatorProxy>,
              CreateProxy,
              (),
              (override));
  MOCK_METHOD(std::optional<DesktopMediaID::Id>,
              GetPipWindowToExcludeFromScreenCapture,
              (DesktopMediaID::Id),
              (override));
};

class DocumentPictureInPictureWindowControllerImplTest
    : public RenderViewHostTestHarness {
 public:
  DocumentPictureInPictureWindowControllerImplTest() = default;
  ~DocumentPictureInPictureWindowControllerImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    mock_pip_screen_capture_coordinator_ =
        std::make_unique<testing::NiceMock<MockPipScreenCaptureCoordinator>>();
    PipScreenCaptureCoordinator::SetInstanceForTesting(
        mock_pip_screen_capture_coordinator_.get());

    controller_ =
        DocumentPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
            web_contents());
  }

  void TearDown() override {
    // `controller_` is a WebContentsUserData, so it's owned by
    // `web_contents()` and will be cleaned up automatically. We don't
    // need to delete it. Null out the raw_ptr members to avoid dangling
    // pointer errors.
    controller_ = nullptr;
    PipScreenCaptureCoordinator::SetInstanceForTesting(nullptr);
    mock_pip_screen_capture_coordinator_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<MockPipScreenCaptureCoordinator>
      mock_pip_screen_capture_coordinator_;
  raw_ptr<DocumentPictureInPictureWindowControllerImpl> controller_;
};

TEST_F(DocumentPictureInPictureWindowControllerImplTest,
       OnVisibilityChangedNotifiesPipScreenCaptureCoordinator) {
  std::unique_ptr<TestWebContents> child_web_contents =
      TestWebContents::Create(browser_context(), nullptr);
  controller_->SetChildWebContents(child_web_contents.get());

  EXPECT_CALL(*mock_pip_screen_capture_coordinator_,
              OnPipShown(testing::Ref(*child_web_contents),
                         web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
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
