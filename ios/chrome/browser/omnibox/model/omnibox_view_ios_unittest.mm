// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"

#import <stddef.h>

#import <array>
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
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_text_util.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_data.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_popup_view_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_view_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/ui_base_features.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/favicon_size.h"
#import "ui/gfx/image/image_unittest_util.h"
#import "ui/gfx/paint_vector_icon.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SaveArgPointee;

namespace {

class OmniboxViewIOSTest : public PlatformTest {
 public:
  OmniboxViewIOSTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();
    EXPECT_CALL(*client(), GetBookmarkModel())
        .WillRepeatedly(Return(bookmark_model_.get()));

    view_ = std::make_unique<TestOmniboxViewIOS>(std::move(omnibox_client));
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModelIOS>(view_->controller(),
                                                  view_.get(),
                                                  /*pref_service=*/nullptr));
  }

  TestOmniboxViewIOS* view() { return view_.get(); }

  TestOmniboxEditModelIOS* model() {
    return static_cast<TestOmniboxEditModelIOS*>(view_->model());
  }

  TestOmniboxClient* client() { return omnibox_client_; }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestOmniboxViewIOS> view_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
};

class OmniboxViewIOSPopupTest : public PlatformTest {
 public:
  OmniboxViewIOSPopupTest() {
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();

    view_ = std::make_unique<TestOmniboxViewIOS>(std::move(omnibox_client));
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModelIOS>(view_->controller(),
                                                  view_.get(),
                                                  /*pref_service=*/nullptr));
    model()->set_popup_view(&popup_view_);
    model()->SetPopupIsOpen(true);
  }

  TestOmniboxViewIOS* view() { return view_.get(); }

  TestOmniboxEditModelIOS* model() {
    return static_cast<TestOmniboxEditModelIOS*>(view_->model());
  }

  TestOmniboxClient* client() { return omnibox_client_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestOmniboxViewIOS> view_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  TestOmniboxPopupViewIOS popup_view_;
};
}  // namespace

// Tests GetStateChanges correctly determines if text was deleted.
TEST_F(OmniboxViewIOSTest, GetStateChanges_DeletedText) {
  {
    // Continuing autocompletion
    auto state_before =
        TestOmniboxViewIOS::CreateState("google.com", 10, 3);  // goo[gle.com]
    auto state_after = TestOmniboxViewIOS::CreateState("goog", 4, 4);  // goog|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion
    auto state_before =
        TestOmniboxViewIOS::CreateState("google.com", 1, 10);  // g[oogle.com]
    auto state_after = TestOmniboxViewIOS::CreateState("gi", 2, 2);  // gi|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion
    auto state_before =
        TestOmniboxViewIOS::CreateState("google.com", 1, 10);  // g[oogle.com]
    auto state_after = TestOmniboxViewIOS::CreateState("g", 1, 1);  // g|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Inserting
    auto state_before =
        TestOmniboxViewIOS::CreateState("goole.com", 3, 3);  // goo|le.com
    auto state_after =
        TestOmniboxViewIOS::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting
    auto state_before =
        TestOmniboxViewIOS::CreateState("googgle.com", 5, 5);  // googg|le.com
    auto state_after =
        TestOmniboxViewIOS::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Replacing
    auto state_before =
        TestOmniboxViewIOS::CreateState("goojle.com", 3, 4);  // goo[j]le.com
    auto state_after =
        TestOmniboxViewIOS::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
}
