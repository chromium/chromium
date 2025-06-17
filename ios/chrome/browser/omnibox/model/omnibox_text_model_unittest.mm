// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class OmniboxTextModelTest : public PlatformTest {
 protected:
  OmniboxTextModelTest() : model_(&client_) {}

  // Helper to create OmniboxTextState for tests.
  static OmniboxTextState CreateState(const std::string& text_utf8,
                                      size_t sel_start,
                                      size_t sel_end) {
    OmniboxTextState state;
    state.text = base::UTF8ToUTF16(text_utf8);
    state.sel_start = sel_start;
    state.sel_end = sel_end;
    return state;
  }

 public:
  base::test::TaskEnvironment task_environment_;
  TestOmniboxClient client_;
  OmniboxTextModel model_;
};

// Tests GetStateChanges correctly determines if text was deleted.
TEST_F(OmniboxTextModelTest, GetStateChanges_DeletedText) {
  {
    // Continuing autocompletion
    auto state_before = CreateState("google.com", 10, 3);  // goo[gle.com]
    auto state_after = CreateState("goog", 4, 4);          // goog|
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion
    auto state_before = CreateState("google.com", 1, 10);  // g[oogle.com]
    auto state_after = CreateState("gi", 2, 2);            // gi|
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion
    auto state_before = CreateState("google.com", 1, 10);  // g[oogle.com]
    auto state_after = CreateState("g", 1, 1);             // g|
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Inserting
    auto state_before = CreateState("goole.com", 3, 3);  // goo|le.com
    auto state_after = CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting
    auto state_before = CreateState("googgle.com", 5, 5);  // googg|le.com
    auto state_after = CreateState("google.com", 4, 4);    // goog|le.com
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Replacing
    auto state_before = CreateState("goojle.com", 3, 4);  // goo[j]le.com
    auto state_after = CreateState("google.com", 4, 4);   // goog|le.com
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Replacing with shorter text, but cursor moved past original selection
    // start (e.g. select "jkl" in "ghijkl" and type "x") "ghi[jkl]mno"
    // (sel_start=3, sel_end=6) -> "ghixmno" (sel_start=4, sel_end=4) This
    // should NOT be `just_deleted_text`.
    auto state_before = CreateState("ghijklmno", 3, 6);
    auto state_after = CreateState("ghixmno", 4, 4);
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Replacing with shorter text, cursor at original selection start
    // (e.g. select "jkl" in "ghijkl" and type "x", then backspace to put cursor
    // at start of "x") "ghi[jkl]mno" (sel_start=3, sel_end=6) -> "ghixmno"
    // (sel_start=3, sel_end=3) This should be `just_deleted_text` because the
    // new text is shorter AND the cursor is at or before the original
    // selection's start.
    auto state_before = CreateState("ghijklmno", 3, 6);
    auto state_after = CreateState("ghixmno", 3, 3);  // Cursor at 'x'
    auto state_changes = model_.GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
}
