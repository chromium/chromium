// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_

#import <string_view>
#import <unordered_map>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class TabGroup;
class WebStateList;

namespace web {
class WebState;
}

// Helper to build a WebStateList from a simple description e.g. the following
// code
//
//   WebStateList web_state_list(...);
//   WebStateListBuilderFromDescription builder(&web_state_list);
//   ASSERT_TRUE(builder.BuildWebStateListFromDescription(
//       "a b | [ 0 c d* ] e f [ 1 g h ]"));
//
// will initialize `web_state_list` with two pinned tabs "a" and "b", six
// regular tabs "c", "d", "e", "f", "g", "h" where "d" is active, and two tab
// groups: the first tab group has identifier "0" and contains tabs "c" and "d",
// the second tab group has identifier "1" and contains tabs "g" and "h".
// The `builder` object observes the WebStateList and memorizes the identifiers
// associated with each tab and tab group, such that the result of operations
// performed on `web_state_list` can be checked with e.g. the following code:
//
//   web_state_list.SetWebStatePinnedAt(4, true);
//   web_state_list.CloseWebStateAt(0, WebStateList::CLOSE_NO_FLAGS);
//   EXPECT_EQ("b e | [ 0 c d* ] f [ 1 g h ]",
//             builder.GetWebStateListDescription());
//
// checks that "e" was pinned and "a" removed.
class WebStateListBuilderFromDescription : public WebStateListObserver {
  using WebState = web::WebState;

 public:
  // `web_state_list` must be empty. The WebStateListBuilderFromDescription will
  // observe its `web_state_list`, so it must not outlive it.
  WebStateListBuilderFromDescription(WebStateList* web_state_list);
  ~WebStateListBuilderFromDescription() override;

  // Initializes `web_state_list_` using `description` as a blueprint.
  // Returns `false` if `description` is not valid, in which case the state of
  // `web_state_list_` is unspecified.
  [[nodiscard]] bool BuildWebStateListFromDescription(
      std::string_view description,
      ProfileIOS* profile = nullptr);
  [[nodiscard]] bool BuildWebStateListFromDescription(
      std::string_view description,
      base::RepeatingCallback<std::unique_ptr<web::WebState>()>
          create_web_state);

  // Returns the description of `web_state_list_`.
  std::string GetWebStateListDescription() const;

  // Formats `description` to match `GetWebStateListDescription()`
  // e.g. if `description` is valid, then the following code should pass.
  //
  //   ASSERT_TRUE(builder.BuildWebStateListFromDescription(description));
  //   EXPECT_EQ(builder.FormatWebStateListDescription(description),
  //             builder.GetWebStateListDescription());
  //
  std::string FormatWebStateListDescription(std::string_view description) const;

  // Returns the WebState associated with identifier `identifier`, if any.
  WebState* GetWebStateForIdentifier(char identifier) const;

  // Returns the TabGroup associated with identifier `identifier`, if any.
  const TabGroup* GetTabGroupForIdentifier(char identifier) const;

  // Returns the identifiers associated with a WebState or TabGroup.
  // If no identifier exists for this element, then the placeholder character
  // '_' is returned instead.
  char GetWebStateIdentifier(WebState* web_state) const;
  char GetTabGroupIdentifier(const TabGroup* tab_group) const;

  // Sets the identifier for a WebState or TabGroup. The identifier must be
  // valid i.e. a lowercase/uppercase letter for a WebState, a digit for a
  // TabGroup. The identifier cannot be already used.
  void SetWebStateIdentifier(WebState* web_state, char identifier);
  void SetTabGroupIdentifier(const TabGroup* tab_group, char identifier);

  // Generates identifiers for WebStates and TabGroups in `web_state_list_`
  // which have not been named yet.
  void GenerateIdentifiersForWebStateList();

  // WebStateListObserver methods.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  void WebStateListDestroyed(WebStateList* web_state_list) override;

 private:
  // The observed WebStateList.
  raw_ptr<WebStateList> web_state_list_;
  // Memorizes identifiers for WebStates and TabGroups created by this builder.
  std::unordered_map<char, raw_ptr<WebState>> web_state_for_identifier_;
  std::unordered_map<char, raw_ptr<const TabGroup>> tab_group_for_identifier_;
  std::unordered_map<raw_ptr<WebState>, char> identifier_for_web_state_;
  std::unordered_map<raw_ptr<const TabGroup>, char> identifier_for_tab_group_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_WEB_STATE_LIST_BUILDER_FROM_DESCRIPTION_H_
