// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_

#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"

namespace web {

class WebState;

// Use this as the delegate for FindInPageManager responses in test suites.
class FakeFindInPageManagerDelegate : public FindInPageManagerDelegate {
 public:
  FakeFindInPageManagerDelegate();

  FakeFindInPageManagerDelegate(const FakeFindInPageManagerDelegate&) = delete;
  FakeFindInPageManagerDelegate& operator=(
      const FakeFindInPageManagerDelegate&) = delete;

  ~FakeFindInPageManagerDelegate() override;

  // FindInPageManagerDelegate override
  void DidHighlightMatches(AbstractFindInPageManager* manager,
                           WebState* web_state,
                           int match_count,
                           NSString* query) override;
  void DidSelectMatch(AbstractFindInPageManager* manager,
                      WebState* web_state,
                      int index,
                      NSString* context_string) override;
  void UserDismissedFindNavigator(AbstractFindInPageManager* manager) override;

  // Holds the state passed to DidHighlightMatches and DidSelectMatch.
  struct State {
    State();
    ~State();
    raw_ptr<WebState> web_state = nullptr;
    int match_count = -1;
    NSString* query;
    int index = -1;
    NSString* context_string;
    bool user_dismissed_find_navigator = false;
  };

  // Returns the current State.
  const State* state() const { return delegate_state_.get(); }

  // Resets the State.
  void Reset() { delegate_state_.reset(); }

 private:
  std::unique_ptr<State> delegate_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
