// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"

namespace web {

FakeFindInPageManagerDelegate::State::State() = default;

FakeFindInPageManagerDelegate::State::~State() = default;

FakeFindInPageManagerDelegate::FakeFindInPageManagerDelegate() = default;

FakeFindInPageManagerDelegate::~FakeFindInPageManagerDelegate() = default;

void FakeFindInPageManagerDelegate::DidHighlightMatches(
    AbstractFindInPageManager* manager,
    WebState* web_state,
    int match_count,
    NSString* query) {
  if (!delegate_state_) {
    delegate_state_ = std::make_unique<State>();
  }
  delegate_state_->web_state = web_state;
  delegate_state_->match_count = match_count;
  delegate_state_->query = query;
}

void FakeFindInPageManagerDelegate::DidSelectMatch(
    AbstractFindInPageManager* manager,
    WebState* web_state,
    int index,
    NSString* context_string) {
  if (!delegate_state_) {
    delegate_state_ = std::make_unique<State>();
  }
  delegate_state_->web_state = web_state;
  delegate_state_->index = index;
  delegate_state_->context_string = context_string;
}

void FakeFindInPageManagerDelegate::UserDismissedFindNavigator(
    AbstractFindInPageManager* manager) {
  if (!delegate_state_) {
    delegate_state_ = std::make_unique<State>();
  }
  delegate_state_->user_dismissed_find_navigator = true;
}

}  // namespace web
