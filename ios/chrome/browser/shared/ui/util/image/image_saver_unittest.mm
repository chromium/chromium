// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/image/image_saver.h"

#import <Photos/Photos.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/run_until.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

const char kTestImageUrl[] = "https://www.example.com/test_image.gif";

// Fake implementation of ImageFetchTabHelper that supplies test image data.
class FakeImageFetchTabHelper : public ImageFetchTabHelper {
 public:
  static FakeImageFetchTabHelper* CreateForWebState(web::WebState* web_state) {
    auto helper = std::make_unique<FakeImageFetchTabHelper>(web_state);
    FakeImageFetchTabHelper* helper_ptr = helper.get();
    web_state->SetUserData(UserDataKey(), std::move(helper));
    return helper_ptr;
  }

  explicit FakeImageFetchTabHelper(web::WebState* web_state)
      : ImageFetchTabHelper(web_state) {}
  FakeImageFetchTabHelper(const FakeImageFetchTabHelper&) = delete;
  FakeImageFetchTabHelper& operator=(const FakeImageFetchTabHelper&) = delete;
  ~FakeImageFetchTabHelper() override = default;

  void SetImageData(NSData* data) { image_data_ = data; }

  bool GetImageDataCalled() const { return get_image_data_called_; }

  void GetImageData(const GURL& url,
                    const web::Referrer& referrer,
                    const std::string& frame_id,
                    const url::Origin& frame_origin,
                    ImageDataCallback callback) override {
    get_image_data_called_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback, image_data_));
  }

 private:
  bool get_image_data_called_ = false;
  NSData* image_data_ = nil;
};

}  // namespace

class ImageSaverTest : public PlatformTest {
 protected:
  ImageSaverTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_ = std::make_unique<web::FakeWebState>();
    tab_helper_ = FakeImageFetchTabHelper::CreateForWebState(web_state_.get());

    mock_photo_library_ = OCMClassMock([PHPhotoLibrary class]);
    OCMStub([mock_photo_library_ sharedPhotoLibrary])
        .andReturn(mock_photo_library_);

    mock_base_view_controller_ = OCMClassMock([UIViewController class]);
    image_saver_ = [[ImageSaver alloc] initWithBrowser:browser_.get()];
  }

  void TearDown() override {
    [image_saver_ stop];
    [mock_photo_library_ stopMocking];
    [mock_base_view_controller_ stopMocking];
    PlatformTest::TearDown();
  }

  void ExpectAlertPresented(NSString* expected_title,
                            NSString* expected_message) {
    alert_presented_ = false;
    OCMExpect([mock_base_view_controller_
                  presentViewController:[OCMArg checkWithBlock:^BOOL(id obj) {
                    UIAlertController* alert =
                        base::apple::ObjCCast<UIAlertController>(obj);
                    return [alert.title isEqualToString:expected_title] &&
                           [alert.message isEqualToString:expected_message];
                  }]
                               animated:YES
                             completion:nil])
        .andDo(^(NSInvocation*) {
          alert_presented_ = true;
        });
  }

  void WaitForAlert() {
    ASSERT_TRUE(base::test::RunUntil([&]() { return alert_presented_; }));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<FakeImageFetchTabHelper> tab_helper_ = nullptr;
  id mock_photo_library_ = nil;
  id mock_base_view_controller_ = nil;
  ImageSaver* image_saver_ = nil;
  bool alert_presented_ = false;
};

// Tests that attempting to save an invalid image presents the save error alert.
TEST_F(ImageSaverTest, SaveInvalidImagePresentsErrorAlert) {
  NSData* invalid_image_data =
      [@"not_a_valid_image" dataUsingEncoding:NSUTF8StringEncoding];
  tab_helper_->SetImageData(invalid_image_data);

  OCMStub([mock_photo_library_
              authorizationStatusForAccessLevel:PHAccessLevelAddOnly])
      .andReturn(PHAuthorizationStatusAuthorized);

  OCMStub([mock_photo_library_ performChanges:[OCMArg any]
                            completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_handler)(BOOL, NSError*);
        [invocation getArgument:&completion_handler atIndex:3];
        if (completion_handler) {
          NSError* error = [NSError errorWithDomain:PHPhotosErrorDomain
                                               code:PHPhotosErrorInvalidResource
                                           userInfo:nil];
          completion_handler(NO, error);
        }
      });

  ExpectAlertPresented(
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE),
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_ERROR));

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  WaitForAlert();
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
}

// Tests that requesting to save an image with empty data presents the no
// internet alert.
TEST_F(ImageSaverTest, SaveImageWithEmptyDataPresentsNoInternetAlert) {
  tab_helper_->SetImageData([NSData data]);

  ExpectAlertPresented(
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE),
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_NO_INTERNET_CONNECTION));

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  WaitForAlert();
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
  EXPECT_TRUE(tab_helper_->GetImageDataCalled());
}

// Tests that when saving encounters an error and photo access is denied,
// the settings alert is presented.
TEST_F(ImageSaverTest,
       SaveImageWithErrorWhenAccessDeniedPresentsSettingsAlert) {
  tab_helper_->SetImageData(
      [@"image_data" dataUsingEncoding:NSUTF8StringEncoding]);

  OCMStub([mock_photo_library_
              authorizationStatusForAccessLevel:PHAccessLevelAddOnly])
      .andReturn(PHAuthorizationStatusDenied);

  OCMStub([mock_photo_library_ performChanges:[OCMArg any]
                            completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_handler)(BOOL, NSError*);
        [invocation getArgument:&completion_handler atIndex:3];
        if (completion_handler) {
          NSError* error = [NSError errorWithDomain:@"TestPhotoErrorDomain"
                                               code:1
                                           userInfo:nil];
          completion_handler(NO, error);
        }
      });

  ExpectAlertPresented(
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE_GO_TO_SETTINGS));

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  WaitForAlert();
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
}

// Tests that when saving encounters an error and photo access is restricted,
// the settings alert is presented.
TEST_F(ImageSaverTest,
       SaveImageWithErrorWhenAccessRestrictedPresentsSettingsAlert) {
  tab_helper_->SetImageData(
      [@"image_data" dataUsingEncoding:NSUTF8StringEncoding]);

  OCMStub([mock_photo_library_
              authorizationStatusForAccessLevel:PHAccessLevelAddOnly])
      .andReturn(PHAuthorizationStatusRestricted);

  OCMStub([mock_photo_library_ performChanges:[OCMArg any]
                            completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_handler)(BOOL, NSError*);
        [invocation getArgument:&completion_handler atIndex:3];
        if (completion_handler) {
          NSError* error = [NSError errorWithDomain:@"TestPhotoErrorDomain"
                                               code:1
                                           userInfo:nil];
          completion_handler(NO, error);
        }
      });

  ExpectAlertPresented(
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE_GO_TO_SETTINGS));

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  WaitForAlert();
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
}

// Tests that when saving encounters an error but photo access is authorized,
// the generic save error alert is presented.
TEST_F(ImageSaverTest,
       SaveImageWithErrorWhenAccessAuthorizedPresentsGeneralErrorAlert) {
  tab_helper_->SetImageData(
      [@"image_data" dataUsingEncoding:NSUTF8StringEncoding]);

  OCMStub([mock_photo_library_
              authorizationStatusForAccessLevel:PHAccessLevelAddOnly])
      .andReturn(PHAuthorizationStatusAuthorized);

  OCMStub([mock_photo_library_ performChanges:[OCMArg any]
                            completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_handler)(BOOL, NSError*);
        [invocation getArgument:&completion_handler atIndex:3];
        if (completion_handler) {
          NSError* error = [NSError errorWithDomain:@"TestPhotoErrorDomain"
                                               code:2
                                           userInfo:nil];
          completion_handler(NO, error);
        }
      });

  ExpectAlertPresented(
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE),
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_ERROR));

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  WaitForAlert();
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
}

// Tests that successfully saving an image presents no alert.
TEST_F(ImageSaverTest, SaveImageSuccessPresentsNoAlert) {
  tab_helper_->SetImageData(
      [@"image_data" dataUsingEncoding:NSUTF8StringEncoding]);

  auto perform_changes_called = std::make_shared<bool>(false);
  OCMStub([mock_photo_library_ performChanges:[OCMArg any]
                            completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        *perform_changes_called = true;
        void (^completion_handler)(BOOL, NSError*);
        [invocation getArgument:&completion_handler atIndex:3];
        if (completion_handler) {
          completion_handler(YES, nil);
        }
      });

  [[mock_base_view_controller_ reject] presentViewController:[OCMArg any]
                                                    animated:YES
                                                  completion:[OCMArg any]];

  [image_saver_ saveImageAtURL:GURL(kTestImageUrl)
                      referrer:web::Referrer()
                      webState:web_state_.get()
                       frameID:"frame_1"
                   frameOrigin:url::Origin()
            baseViewController:mock_base_view_controller_];

  ASSERT_TRUE(base::test::RunUntil([&]() { return *perform_changes_called; }));
  EXPECT_TRUE(tab_helper_->GetImageDataCalled());
  EXPECT_OCMOCK_VERIFY(mock_base_view_controller_);
}
