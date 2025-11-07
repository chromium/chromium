// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMArg.h"
#import "third_party/ocmock/OCMock/OCMStubRecorder.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

// Test suite for FileUploadPanelMediator.
class FileUploadPanelMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    ChooseFileEvent event =
        ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    handler_ = OCMProtocolMock(@protocol(FileUploadPanelCommands));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeChooseFileController> controller_;
  id<FileUploadPanelCommands> handler_;
};

// Tests that destroying the controller calls the handler to hide the panel.
TEST_F(FileUploadPanelMediatorTest, ControllerDestroyed) {
  if (@available(iOS 18.4, *)) {
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    OCMExpect([handler_ hideFileUploadPanel]);
    controller_.reset();
    EXPECT_OCMOCK_VERIFY(handler_);
  }
}

// Tests that `shouldShowCamera` returns true when the capture type is not
// `kNone` and the camera is available.
TEST_F(FileUploadPanelMediatorTest, ShouldShowCamera) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kUser)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;

    id mockImagePicker = OCMClassMock([UIImagePickerController class]);
    OCMStub([mockImagePicker
                isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        .andReturn(YES);

    EXPECT_NE(ChooseFileCaptureType::kNone, mediator.eventCaptureType);
    EXPECT_TRUE(mediator.shouldShowCamera);

    [mockImagePicker stopMocking];
  }
}

// Tests that `shouldShowCamera` returns false when the camera is not available.
TEST_F(FileUploadPanelMediatorTest, ShouldShowCamera_CameraNotAvailable) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kUser)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;

    id mockImagePicker = OCMClassMock([UIImagePickerController class]);
    OCMStub([mockImagePicker
                isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        .andReturn(NO);

    EXPECT_NE(ChooseFileCaptureType::kNone, mediator.eventCaptureType);
    EXPECT_FALSE(mediator.shouldShowCamera);

    [mockImagePicker stopMocking];
  }
}

// Tests that `shouldShowCamera` returns false when the capture type is `kNone`.
TEST_F(FileUploadPanelMediatorTest, ShouldNotShowCamera) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kNone)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_FALSE(mediator.shouldShowCamera);
  }
}

// Tests that `preferredCameraDevice` returns the front camera when the capture
// type is `kUser`.
TEST_F(FileUploadPanelMediatorTest, PreferredCameraDeviceUser) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kUser)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_EQ(UIImagePickerControllerCameraDeviceFront,
              mediator.preferredCameraDevice);
  }
}

// Tests that `preferredCameraDevice` returns the rear camera when the capture
// type is `kEnvironment`.
TEST_F(FileUploadPanelMediatorTest, PreferredCameraDeviceEnvironment) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kEnvironment)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_EQ(UIImagePickerControllerCameraDeviceRear,
              mediator.preferredCameraDevice);
  }
}

// Tests that `adjustCaptureTypeToAvailableDevices` adjusts the capture type
// when no camera is available.
TEST_F(FileUploadPanelMediatorTest, AdjustCaptureTypeToAvailableDevices) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetCapture(ChooseFileCaptureType::kUser)
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;

    id mockImagePicker = OCMClassMock([UIImagePickerController class]);
    OCMStub(
        [mockImagePicker
            isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceFront])
        .andReturn(NO);
    OCMStub(
        [mockImagePicker
            isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear])
        .andReturn(NO);

    EXPECT_EQ(ChooseFileCaptureType::kUser, mediator.eventCaptureType);
    [mediator adjustCaptureTypeToAvailableDevices];
    EXPECT_EQ(ChooseFileCaptureType::kNone, mediator.eventCaptureType);

    [mockImagePicker stopMocking];
  }
}

// Tests that `allowsImageSelection` and `allowsVideoSelection` return the
// correct values based on the `accept` attribute.
TEST_F(FileUploadPanelMediatorTest, AllowsImageAndVideoSelection) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetAcceptMimeTypes({"image/jpeg", "video/mp4"})
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_TRUE(mediator.allowsImageSelection);
    EXPECT_TRUE(mediator.allowsVideoSelection);
  }
}

// Tests that `allowsVideoSelection` returns false when only images are
// accepted.
TEST_F(FileUploadPanelMediatorTest, DoesNotAllowVideoSelection) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetAcceptMimeTypes({"image/jpeg"})
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_TRUE(mediator.allowsImageSelection);
    EXPECT_FALSE(mediator.allowsVideoSelection);
  }
}

// Tests that `allowsImageSelection` returns false when only videos are
// accepted.
TEST_F(FileUploadPanelMediatorTest, DoesNotAllowImageSelection) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetAcceptMimeTypes({"video/mp4"})
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_FALSE(mediator.allowsImageSelection);
    EXPECT_TRUE(mediator.allowsVideoSelection);
  }
}

// Tests that `allowsMediaSelection` returns false when a non-media type is
// accepted.
TEST_F(FileUploadPanelMediatorTest, DoesNotAllowMediaSelection) {
  if (@available(iOS 18.4, *)) {
    ChooseFileEvent event = ChooseFileEvent::Builder()
                                .SetWebState(web_state_.get())
                                .SetAcceptMimeTypes({"application/pdf"})
                                .Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_FALSE(mediator.allowsImageSelection);
    EXPECT_FALSE(mediator.allowsVideoSelection);
    EXPECT_FALSE(mediator.allowsMediaSelection);
  }
}

// Tests that submitting an image from the camera writes the image to a
// temporary file and submits the file URL.
TEST_F(FileUploadPanelMediatorTest, SubmitImageSelectionFromCamera) {
  if (@available(iOS 18.4, *)) {
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    UIImage* image = ImageWithColor([UIColor redColor]);
    ASSERT_TRUE(image);

    NSDictionary<UIImagePickerControllerInfoKey, id>* mediaInfo = @{
      UIImagePickerControllerMediaType : UTTypeImage.identifier,
      UIImagePickerControllerOriginalImage : image,
    };

    [mediator submitFileSelectionWithMediaInfo:mediaInfo];

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout,
        /* run_message_loop= */ true, ^{
          return controller_->HasSubmittedSelection();
        }));

    ASSERT_EQ(1u, controller_->submitted_file_urls().count);
    NSURL* submittedURL = controller_->submitted_file_urls()[0];
    EXPECT_TRUE(submittedURL.isFileURL);

    base::FilePath submittedPath = base::apple::NSURLToFilePath(submittedURL);
    EXPECT_TRUE(base::PathExists(submittedPath));
  }
}

// Tests that disconnecting the mediator submits the selection if it has not
// been submitted yet.
TEST_F(FileUploadPanelMediatorTest, DisconnectSubmitsSelection) {
  if (@available(iOS 18.4, *)) {
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;
    EXPECT_FALSE(controller_->HasSubmittedSelection());
    [mediator disconnect];
    EXPECT_TRUE(controller_->HasSubmittedSelection());
    histogram_tester_.ExpectUniqueSample(
        "IOS.FileUploadPanel.SubmittedFileCount", 0, 1);
  }
}

// Tests that submitting a file selection records the correct histogram value.
TEST_F(FileUploadPanelMediatorTest, SubmittedFileCountHistogramCancel) {
  if (@available(iOS 18.4, *)) {
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;

    // Test with 0 files (cancel).
    [mediator cancelFileSelection];
    histogram_tester_.ExpectUniqueSample(
        "IOS.FileUploadPanel.SubmittedFileCount", 0, 1);
  }
}

// Tests that submitting a file selection records the correct histogram value.
TEST_F(FileUploadPanelMediatorTest, SubmittedFileCountHistogramSubmit) {
  if (@available(iOS 18.4, *)) {
    FileUploadPanelMediator* mediator = [[FileUploadPanelMediator alloc]
        initWithChooseFileController:controller_.get()];
    mediator.fileUploadPanelHandler = handler_;

    // Test with 2 files.
    NSArray* fileURLs = @[
      [NSURL URLWithString:@"file:///tmp/file1.txt"],
      [NSURL URLWithString:@"file:///tmp/file2.txt"]
    ];
    [mediator submitFileSelection:fileURLs];
    histogram_tester_.ExpectUniqueSample(
        "IOS.FileUploadPanel.SubmittedFileCount", 2, 1);
  }
}
