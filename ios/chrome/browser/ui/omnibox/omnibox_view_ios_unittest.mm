// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import <stddef.h>

#import <string>
#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "build/build_config.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/omnibox/browser/test_omnibox_edit_model.h"
#import "components/omnibox/browser/test_omnibox_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_consumer.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxViewIOSTest : public PlatformTest {
 public:
  OmniboxViewIOSTest() {
    textfield_ = [[OmniboxTextFieldIOS alloc] init];
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();
    mock_consumer_ =
        [OCMockObject mockForProtocol:@protocol(OmniboxViewConsumer)];

    view_ = std::make_unique<OmniboxViewIOS>(
        textfield_, std::move(omnibox_client), /*profile=*/nullptr,
        /*omnibox_focuser=*/nil, /*focus_delegate=*/nil,
        /*toolbar_commands_handler=*/nil, mock_consumer_,
        /*is_lens_overlay=*/false);
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModel>(view_->controller(), view_.get(),
                                               /*pref_service=*/nullptr));
  }

  ~OmniboxViewIOSTest() override { omnibox_client_ = nullptr; }

 protected:
  base::test::TaskEnvironment task_environment_;

  OmniboxTextFieldIOS* textfield_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  OCMockObject<OmniboxViewConsumer>* mock_consumer_;
  std::unique_ptr<OmniboxViewIOS> view_;
};

// Tests that reverting all edits restore the thumbnail after deletion.
TEST_F(OmniboxViewIOSTest, RevertThumbnailEdit) {
  UIImage* image = [[UIImage alloc] init];

  // Add a thumbnail.
  OCMExpect([mock_consumer_ setThumbnailImage:image]);
  view_->SetThumbnailImage(image);

  // Remove the thumbnail.
  OCMExpect([mock_consumer_ setThumbnailImage:nil]);
  view_->RemoveThumbnail();

  // Revert edits.
  OCMExpect([mock_consumer_ setThumbnailImage:image]);
  view_->RevertAll();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

}  // namespace
