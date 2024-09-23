// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"

#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

using tab_groups::TabGroupId;

// Tests for `WebStateListBuilderFromDescription`.
class WebStateListBuilderFromDescriptionTest : public PlatformTest,
                                               public WebStateListDelegate {
 public:
  // WebStateListDelegate implementation
  void WillAddWebState(web::WebState* web_state) final {}
  void WillActivateWebState(web::WebState* web_state) final {}

 protected:
  // Shorthand for `builder_.BuildWebStateListFromDescription(...)`.
  bool BuildWebStateList(std::string_view description) {
    return builder_.BuildWebStateListFromDescription(description);
  }

  // Shorthand for `builder_.GetWebStateListDescription()`.
  std::string GetDescription() const {
    return builder_.GetWebStateListDescription();
  }

  using InsertionParams = WebStateList::InsertionParams;
  // Shorthand for
  // `web_state_list_.InsertWebState(std::make_unique<FakeWebState>(), ...)`.
  void InsertWebState(const InsertionParams& params) {
    web_state_list_.InsertWebState(std::make_unique<web::FakeWebState>(),
                                   params);
  }

  // Resets `web_state_list_`.
  void Reset() {
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }

  WebStateList web_state_list_{this};
  WebStateListBuilderFromDescription builder_{&web_state_list_};
};

// Test that `WebStateListBuilderFromDescription` builds the expected
// WebStateList from the valid description "a b | [ 0 c d* ] e f [ 1 g h ]".
TEST_F(WebStateListBuilderFromDescriptionTest,
       CanBuildWebStateListFromDescription) {
  constexpr std::string_view valid_description =
      "a b | [ 0 c d* ] e f [ 1 g h ]";
  EXPECT_TRUE(BuildWebStateList(valid_description));
  EXPECT_EQ(valid_description, GetDescription());

  EXPECT_EQ(2, web_state_list_.pinned_tabs_count());
  EXPECT_EQ(6, web_state_list_.regular_tabs_count());
  EXPECT_EQ(3, web_state_list_.active_index());

  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            builder_.GetWebStateForIdentifier('a'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(1),
            builder_.GetWebStateForIdentifier('b'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(2),
            builder_.GetWebStateForIdentifier('c'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(3),
            builder_.GetWebStateForIdentifier('d'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(4),
            builder_.GetWebStateForIdentifier('e'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(5),
            builder_.GetWebStateForIdentifier('f'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(6),
            builder_.GetWebStateForIdentifier('g'));
  EXPECT_EQ(web_state_list_.GetWebStateAt(7),
            builder_.GetWebStateForIdentifier('h'));

  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(2),
            builder_.GetTabGroupForIdentifier('0'));
  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(3),
            builder_.GetTabGroupForIdentifier('0'));
  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(4), nullptr);
  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(5), nullptr);
  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(6),
            builder_.GetTabGroupForIdentifier('1'));
  EXPECT_EQ(web_state_list_.GetGroupOfWebStateAt(7),
            builder_.GetTabGroupForIdentifier('1'));
}

// Tests that the description returned by `GetWebStateListDescription(...)` is
// as expected when a WebStateList is built manually.
TEST_F(WebStateListBuilderFromDescriptionTest, CanGetWebStateListDescription) {
  // Manually populate the WebStateList.
  InsertWebState(InsertionParams::AtIndex(0).Pinned(true));
  InsertWebState(InsertionParams::AtIndex(1).Pinned(true));
  InsertWebState(InsertionParams::AtIndex(2));
  InsertWebState(InsertionParams::AtIndex(3).Activate(true));
  InsertWebState(InsertionParams::AtIndex(4));
  InsertWebState(InsertionParams::AtIndex(5));
  InsertWebState(InsertionParams::AtIndex(6));
  InsertWebState(InsertionParams::AtIndex(7));
  web_state_list_.CreateGroup({2, 3}, {}, TabGroupId::GenerateNew());
  web_state_list_.CreateGroup({6, 7}, {}, TabGroupId::GenerateNew());
  EXPECT_EQ("_ _ | [ _ _ _* ] _ _ [ _ _ _ ]", GetDescription());
  builder_.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("a b | [ 0 c d* ] e f [ 1 g h ]", GetDescription());
}

// Test that `BuildWebStateListFromDescription(description)`
// returns true when given a valid `description` and that the description
// returned by `GetWebStateListDescription()` is consistent with the description
// from which the WebStateList was built.
TEST_F(WebStateListBuilderFromDescriptionTest, ManyValidDescriptions) {
  constexpr std::string_view valid_descriptions[] = {
      "|",
      "a b |",
      "a b* |",
      "a | b*",
      "| a b*",
      "c | a b*",
      "c | a b* d",
      "c | a b* [ 0 d ]",
      "c | a b* [ 0 d e ]",
      "c | [ 1 a b* ] [ 0 d e ]",
      "c | [ 1 a ] b* [ 0 d e ]",
      // Three identical descriptions with different formatting, all valid.
      "| [ 1 a ] t u v [ 2 b* ] q r s [ 0 d e ]",
      "|[1a]tuv[2b*]qrs[0de]",
      "        |[  1a]    tu v [ 2  b* ]q  rs[0 d e]       ",
  };
  for (const auto& valid_description : valid_descriptions) {
    Reset();
    ASSERT_TRUE(BuildWebStateList(valid_description));
    EXPECT_EQ(builder_.FormatWebStateListDescription(valid_description),
              GetDescription());
  }
}

// Test that `BuildWebStateListFromDescription(description)`
// returns false when given an invalid `description`.
TEST_F(WebStateListBuilderFromDescriptionTest, ManyInvalidDescriptions) {
  constexpr std::string_view invalid_descriptions[] = {
      "a b c",  // The pinned WebStates separator '|' must appear exactly once.
      "[ 1 a b ] | c",      // Pinned WebStates cannot be in a group.
      "| [ 1 a [ 0 b ] ]",  // TabGroups cannot be nested.
      "| a a",              // Two WebStates cannot share the same identifier.
      "| [ 0 a ] [ 0 b ]",  // Two TabGroups cannot share the same identifier.
      "| a* b*",    // The description cannot contain several active WebStates.
      "| *a b c",   // The token '*' must be placed after a WebState identifier.
      "a | b | c",  // The pinned WebStates separator '|' can only occur once.
      "| [ 0 ]",    // TabGroups cannot be empty.
      "| 0 a ]",    // TabGroups need an opening bracket.
      "| [ 0 a",    // TabGroups need a closing bracket.
      "| [ a ]",    // TabGroups need an identifier.
      "| [ a 0 ]",  // The identifier for a TabGroup must immediately follow the
                    // opening bracket.
      "| 0 [ a ]",  // The identifier for a TabGroup must immediately follow the
                    // opening bracket.
      "():/+=",     // Invalid tokens.
      "| _",        // Cannot use placeholders to build a WebStateList.
  };
  for (const auto& invalid_description : invalid_descriptions) {
    Reset();
    EXPECT_FALSE(BuildWebStateList(invalid_description))
        << "\nBuildWebStateListFromDescription(...) unexpectedly returned true "
           "for an invalid description."
        << "\nInvalid description: " << invalid_description;
  }
}

// Test that WebStates and TabGroups can be given identifiers using
// `SetWebStateIdentifier(...)`, `SetTabGroupIdentifier(...)` and
// `GenerateIdentifiersForWebStateList(...)`.
TEST_F(WebStateListBuilderFromDescriptionTest, CanSetIdentifiers) {
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_ptr = web_state_1.get();
  web_state_list_.InsertWebState(std::move(web_state_1));
  EXPECT_EQ("| _", GetDescription());
  builder_.SetWebStateIdentifier(web_state_1_ptr, 'a');
  EXPECT_EQ("| a", GetDescription());
  builder_.SetWebStateIdentifier(web_state_1_ptr, 'b');
  EXPECT_EQ("| b", GetDescription());

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_2_ptr = web_state_2.get();
  web_state_list_.InsertWebState(std::move(web_state_2));
  EXPECT_EQ("| b _", GetDescription());
  builder_.SetWebStateIdentifier(web_state_2_ptr, 'a');
  EXPECT_EQ("| b a", GetDescription());
  builder_.SetWebStateIdentifier(web_state_2_ptr, 'c');
  EXPECT_EQ("| b c", GetDescription());

  const TabGroup* tab_group =
      web_state_list_.CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  EXPECT_EQ("| [ _ b c ]", GetDescription());
  builder_.SetTabGroupIdentifier(tab_group, '0');
  EXPECT_EQ("| [ 0 b c ]", GetDescription());
  builder_.SetTabGroupIdentifier(tab_group, '1');
  EXPECT_EQ("| [ 1 b c ]", GetDescription());

  builder_.SetTabGroupIdentifier(tab_group, '9');
  builder_.SetWebStateIdentifier(web_state_1_ptr, 'z');
  builder_.SetWebStateIdentifier(web_state_2_ptr, 'y');
  EXPECT_EQ("| [ 9 z y ]", GetDescription());

  InsertWebState(InsertionParams::Automatic());
  InsertWebState(InsertionParams::Automatic());
  InsertWebState(InsertionParams::Automatic());
  InsertWebState(InsertionParams::Automatic());
  InsertWebState(InsertionParams::Automatic());
  EXPECT_EQ("| [ 9 z y ] _ _ _ _ _", GetDescription());

  builder_.SetWebStateIdentifier(web_state_list_.GetWebStateAt(2), 'x');
  builder_.SetWebStateIdentifier(web_state_list_.GetWebStateAt(4), 'w');
  builder_.SetWebStateIdentifier(web_state_list_.GetWebStateAt(6), 'v');
  EXPECT_EQ("| [ 9 z y ] x _ w _ v", GetDescription());

  builder_.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| [ 9 z y ] x a w b v", GetDescription());

  Reset();
  EXPECT_EQ("|", GetDescription());
  ASSERT_TRUE(BuildWebStateList("| [ 0 a b ] c d e f g"));
  EXPECT_EQ("| [ 0 a b ] c d e f g", GetDescription());
}

// Tests that modifications on a WebStateList are reflected in the description
// returned by `GetWebStateListDescription()`
TEST_F(WebStateListBuilderFromDescriptionTest,
       CanCheckWebStateListModifications) {
  InsertWebState(InsertionParams::AtIndex(0));
  InsertWebState(InsertionParams::AtIndex(1));
  InsertWebState(InsertionParams::AtIndex(2));

  EXPECT_EQ("| _ _ _", GetDescription());
  builder_.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| a b c", GetDescription());

  web_state_list_.SetWebStatePinnedAt(2, true);
  web_state_list_.SetWebStatePinnedAt(2, true);
  web_state_list_.SetWebStatePinnedAt(2, true);
  EXPECT_EQ("c b a |", GetDescription());

  web_state_list_.SetWebStatePinnedAt(0, false);
  web_state_list_.SetWebStatePinnedAt(0, false);
  web_state_list_.SetWebStatePinnedAt(0, false);
  EXPECT_EQ("| c b a", GetDescription());

  web_state_list_.MoveWebStateAt(0, 2);
  web_state_list_.MoveWebStateAt(1, 0);
  EXPECT_EQ("| a b c", GetDescription());

  {
    // Check that there is no sharing with another builder.
    WebStateList other_web_state_list{this};
    WebStateListBuilderFromDescription other_builder(&other_web_state_list);
    EXPECT_EQ("| a b c", builder_.GetWebStateListDescription());
    EXPECT_EQ("|", other_builder.GetWebStateListDescription());
    other_web_state_list.InsertWebState(web_state_list_.DetachWebStateAt(2));
    other_web_state_list.InsertWebState(web_state_list_.DetachWebStateAt(1));
    other_web_state_list.InsertWebState(web_state_list_.DetachWebStateAt(0));
    EXPECT_EQ("|", builder_.GetWebStateListDescription());
    // WebStates have no identifiers that could have leaked from the initial
    // builder.
    EXPECT_EQ("| _ _ _", other_builder.GetWebStateListDescription());
    CloseAllWebStates(other_web_state_list, WebStateList::CLOSE_NO_FLAGS);
    EXPECT_EQ("|", other_builder.GetWebStateListDescription());
  }

  Reset();

  InsertWebState(InsertionParams::Automatic().Pinned(true));
  InsertWebState(InsertionParams::Automatic().Pinned(true));
  InsertWebState(InsertionParams::Automatic());
  InsertWebState(InsertionParams::Automatic().Activate(true));
  InsertWebState(InsertionParams::Automatic());
  EXPECT_EQ("_ _ | _ _* _", GetDescription());
  builder_.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("a b | c d* e", GetDescription());

  web_state_list_.ActivateWebStateAt(WebStateList::kInvalidIndex);
  EXPECT_EQ("a b | c d e", GetDescription());
  web_state_list_.ActivateWebStateAt(0);
  EXPECT_EQ("a* b | c d e", GetDescription());
  web_state_list_.ActivateWebStateAt(1);
  EXPECT_EQ("a b* | c d e", GetDescription());
  web_state_list_.ActivateWebStateAt(2);
  EXPECT_EQ("a b | c* d e", GetDescription());
  web_state_list_.ActivateWebStateAt(3);
  EXPECT_EQ("a b | c d* e", GetDescription());
  web_state_list_.ActivateWebStateAt(4);
  EXPECT_EQ("a b | c d e*", GetDescription());

  web_state_list_.CreateGroup({2}, {}, TabGroupId::GenerateNew());
  EXPECT_EQ("a b | [ _ c ] d e*", GetDescription());
  web_state_list_.CreateGroup({3}, {}, TabGroupId::GenerateNew());
  EXPECT_EQ("a b | [ _ c ] [ _ d ] e*", GetDescription());
  web_state_list_.CreateGroup({4}, {}, TabGroupId::GenerateNew());
  EXPECT_EQ("a b | [ _ c ] [ _ d ] [ _ e* ]", GetDescription());
  builder_.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("a b | [ 0 c ] [ 1 d ] [ 2 e* ]", GetDescription());

  web_state_list_.CloseWebStatesAtIndices(WebStateList::CLOSE_NO_FLAGS,
                                          {0, 2, 4});
  EXPECT_EQ("b | [ 1 d* ]", GetDescription());
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ("b* |", GetDescription());
  CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ("|", GetDescription());
}

// Tests that the identifier of a closed tab doesn’t get reused for a new tab.
TEST_F(WebStateListBuilderFromDescriptionTest, ResetsTabIdentifier) {
  ASSERT_TRUE(BuildWebStateList("| a"));
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(0);
  EXPECT_EQ("|", GetDescription());

  web_state_list_.InsertWebState(std::move(detached_web_state),
                                 InsertionParams::Automatic());

  EXPECT_EQ("| _", GetDescription());
}

// Tests that the identifier of a closed group doesn’t get reused for a new
// group.
TEST_F(WebStateListBuilderFromDescriptionTest, ResetsGroupIdentifier) {
  ASSERT_TRUE(BuildWebStateList("| [ 0 a ]"));
  const TabGroup* group = builder_.GetTabGroupForIdentifier('0');
  web_state_list_.DeleteGroup(group);
  EXPECT_EQ("| a", GetDescription());

  const auto visual_data = tab_groups::TabGroupVisualData(
      u"New Group", tab_groups::TabGroupColorId::kGrey);
  web_state_list_.CreateGroup({0}, visual_data, TabGroupId::GenerateNew());

  EXPECT_EQ("| [ _ a ]", GetDescription());
}

// Tests that replacing a WebState doesn’t change the identifier.
TEST_F(WebStateListBuilderFromDescriptionTest, ReplaceKeepsIdentifier) {
  ASSERT_TRUE(BuildWebStateList("| a"));
  web::WebState* initial_web_state = web_state_list_.GetWebStateAt(0);
  ASSERT_EQ(initial_web_state, web_state_list_.GetWebStateAt(0));

  web_state_list_.ReplaceWebStateAt(0, std::make_unique<web::FakeWebState>());

  // Check that the WebState actually changed.
  EXPECT_NE(initial_web_state, web_state_list_.GetWebStateAt(0));
  // Check that the replacing WebState inherited the identifier.
  EXPECT_EQ("| a", GetDescription());
}

// Tests that detaching a WebState removes its identifier.
TEST_F(WebStateListBuilderFromDescriptionTest, DetachRemovesIdentifier) {
  ASSERT_TRUE(BuildWebStateList("| a"));
  web::WebState* web_state = web_state_list_.GetWebStateAt(0);
  EXPECT_EQ('a', builder_.GetWebStateIdentifier(web_state));

  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(0);

  EXPECT_EQ(detached_web_state.get(), web_state);
  EXPECT_EQ('_', builder_.GetWebStateIdentifier(web_state));
  EXPECT_EQ("|", GetDescription());
}
