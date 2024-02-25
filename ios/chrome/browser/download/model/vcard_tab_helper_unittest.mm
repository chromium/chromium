// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/vcard_tab_helper.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
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
  VcardTabHelperTest() { VcardTabHelper::CreateForWebState(&web_state_); }

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
