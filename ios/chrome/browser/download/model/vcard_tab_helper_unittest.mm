// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/vcard_tab_helper.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

char kUrl[] = "https://test.test/";

}  // namespace

// Test fixture for testing VcardTabHelperTest class.
class VcardTabHelperTest : public PlatformTest {
 protected:
  VcardTabHelperTest() {
    VcardTabHelper::CreateForWebState(&web_state_);
    web_state_.WasShown();
  }

  VcardTabHelper* tab_helper() {
    return VcardTabHelper::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
};

// Tests downloading a valid vcard file.
TEST_F(VcardTabHelperTest, ValidVcardFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kVcardMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data = testing::GetTestFileContents(testing::kVcardFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];

  // Verify that openVcardFromData was correctly dispatched.
  id mockHandler = OCMProtocolMock(@protocol(VcardTabHelperDelegate));
  tab_helper()->set_delegate(mockHandler);
  OCMExpect([mockHandler openVcardFromData:data]);

  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);

  EXPECT_OCMOCK_VERIFY(mockHandler);
}

// Tests downloading a valid vcard file with the older text/x-vcard MIME type.
TEST_F(VcardTabHelperTest, ValidXVcardFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kXVcardMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data = testing::GetTestFileContents(testing::kVcardFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];

  // Verify that openVcardFromData was correctly dispatched.
  id mockHandler = OCMProtocolMock(@protocol(VcardTabHelperDelegate));
  tab_helper()->set_delegate(mockHandler);
  OCMExpect([mockHandler openVcardFromData:data]);

  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);

  EXPECT_OCMOCK_VERIFY(mockHandler);
}

// Tests deferring vcard presentation when the tab is hidden.
TEST_F(VcardTabHelperTest, DeferVcardPresentationWhenHidden) {
  web_state_.WasHidden();

  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kVcardMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data = testing::GetTestFileContents(testing::kVcardFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];

  id mock_handler = OCMProtocolMock(@protocol(VcardTabHelperDelegate));
  tab_helper()->set_delegate(mock_handler);

  // The delegate should not be notified while the web state is hidden.
  [[mock_handler reject] openVcardFromData:OCMOCK_ANY];

  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);

  EXPECT_OCMOCK_VERIFY(mock_handler);

  // Now, show the web state. The delegate should be called.
  id mock_handler_visible = OCMProtocolMock(@protocol(VcardTabHelperDelegate));
  tab_helper()->set_delegate(mock_handler_visible);
  OCMExpect([mock_handler_visible openVcardFromData:data]);

  web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(mock_handler_visible);
}
