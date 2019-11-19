// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"

namespace web {

class WebState;

// Use this as the delegate for FindInPageManager responses in test suites.
class FakeFindInPageManagerDelegate : public FindInPageManagerDelegate {
 public:
  FakeFindInPageManagerDelegate();
  ~FakeFindInPageManagerDelegate() override;

  // FindInPageManagerDelegate override
  void DidHighlightMatches(WebState* web_state,
                           int match_count,
                           NSString* query) override;
  void DidSelectMatch(WebState* web_state,
                      int index,
                      NSString* context_string) override;

  // Holds the state passed to DidHighlightMatches and DidSelectMatch.
  struct State {
    State();
    ~State();
    WebState* web_state = nullptr;
    int match_count = -1;
    NSString* query;
    int index = -1;
    NSString* context_string;
  };

  // Returns the current State.
  const State* state() const { return delegate_state_.get(); }

  // Resets the State.
  void Reset() { delegate_state_.reset(); }

 private:
  std::unique_ptr<State> delegate_state_;
  DISALLOW_COPY_AND_ASSIGN(FakeFindInPageManagerDelegate);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
