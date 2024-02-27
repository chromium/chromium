// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_

#import <string_view>
#import <unordered_map>

#import "base/memory/raw_ptr.h"

class TabGroup;

namespace web {
class WebState;
}

class WebStateList;

// Helper to build a WebStateList from a simple description e.g. the following
// code
//
//   WebStateList web_state_list(...);
//   WebStateListBuilderFromDescription builder;
//   ASSERT_TRUE(builder.BuildWebStateListFromDescription(
//       web_state_list, "a b | [ 0 c d* ] e f [ 1 g h ]"));
//
// will initialize `web_state_list` with two pinned tabs "a" and "b", six
// regular tabs "c", "d", "e", "f", "g", "h" where "d" is active, and two tab
// groups: the first tab group has identifier "0" and contains tabs "c" and "d",
// the second tab group has identifier "1" and contains tabs "g" and "h".
// The `builder` object memorizes the identifiers associated with each tab and
// tab group, such that the result of operations performed on `web_state_list`
// can be checked e.g. the following code
//
//   web_state_list.SetWebStatePinnedAt(4, true);
//   web_state_list.CloseWebStateAt(0, WebStateList::CLOSE_NO_FLAGS);
//   EXPECT_EQ("b e | [ 0 c d* ] f [ 1 g h ]",
//             builder.GetWebStateListDescription(web_state_list));
//
// checks that "e" was pinned and "a" removed.
class WebStateListBuilderFromDescription {
  using WebState = web::WebState;

 public:
  WebStateListBuilderFromDescription();
  ~WebStateListBuilderFromDescription();

  // Initializes `web_state_list` using `description` as a blueprint.
  // Returns `false` if `description` is not valid, in which case the state of
  // `web_state_list` is unspecified.
  [[nodiscard]] bool BuildWebStateListFromDescription(
      WebStateList& web_state_list,
      std::string_view description);

  // Returns the description for a given `web_state_list`.
  std::string GetWebStateListDescription(
      const WebStateList& web_state_list) const;

  // Formats `description` to match `GetWebStateListDescription(web_state_list)`
  // e.g. if `description` is valid, then the following code should pass.
  //
  //   ASSERT_TRUE(builder.BuildWebStateListFromDescription(web_state_list,
  //                                                        description));
  //   EXPECT_EQ(builder.FormatWebStateListDescription(description),
  //             builder.GetWebStateListDescription(web_state_list));
  //
  std::string FormatWebStateListDescription(std::string_view description) const;

  // Returns the WebState associated with identifier `identifier`, if any.
  const WebState* GetWebStateForIdentifier(char identifier) const;

  // Returns the TabGroup associated with identifier `identifier`, if any.
  const TabGroup* GetTabGroupForIdentifier(char identifier) const;

  // Returns the identifiers associated with a WebState or TabGroup.
  // If no identifier exists for this element, then the placeholder character
  // '_' is returned instead.
  char GetWebStateIdentifier(const WebState* web_state) const;
  char GetTabGroupIdentifier(const TabGroup* tab_group) const;

  // Sets the identifier for a WebState or TabGroup. The identifier must be
  // valid i.e. a lowercase/uppercase letter for a WebState, a digit for a
  // TabGroup. The identifier cannot be already used.
  void SetWebStateIdentifier(const WebState* web_state, char identifier);
  void SetTabGroupIdentifier(const TabGroup* tab_group, char identifier);

  // Generates identifiers for WebStates and TabGroups in `web_state_list` which
  // have not be named yet.
  void GenerateIdentifiersForWebStateList(const WebStateList& web_state_list);

 private:
  // Memorizes identifiers for WebStates and TabGroups created by this builder.
  std::unordered_map<char, raw_ptr<const WebState>> web_state_for_identifier_;
  std::unordered_map<char, raw_ptr<const TabGroup>> tab_group_for_identifier_;
  std::unordered_map<raw_ptr<const WebState>, char> identifier_for_web_state_;
  std::unordered_map<raw_ptr<const TabGroup>, char> identifier_for_tab_group_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_
