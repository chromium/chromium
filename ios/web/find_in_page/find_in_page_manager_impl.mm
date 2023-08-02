// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/web/find_in_page/find_in_page_manager_impl.h"
#import "ios/web/find_in_page/find_in_page_metrics.h"
#import "ios/web/public/find_in_page/crw_find_interaction.h"
#import "ios/web/public/find_in_page/crw_find_session.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"

namespace {

// Default delay between each call to `PollActiveFindSession`.
auto kPollActiveFindSessionDelay = base::Milliseconds(100);

}  // namespace

namespace web {

FindInPageManagerImpl::FindInPageManagerImpl(web::WebState* web_state)
    : poll_active_find_session_delay_(kPollActiveFindSessionDelay),
      web_state_(web_state),
      weak_factory_(this) {
  web_state_->AddObserver(this);
  find_session_polling_timer_.SetTaskRunner(GetUIThreadTaskRunner({}));
}

FindInPageManagerImpl::~FindInPageManagerImpl() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void FindInPageManagerImpl::Find(NSString* query, FindInPageOptions options) {
  if (@available(iOS 16, *)) {
    switch (options) {
      case FindInPageOptions::FindInPageSearch:
        DCHECK(query);
        StartSearch(query);
        break;
      case FindInPageOptions::FindInPageNext:
        SelectNextMatch();
        break;
      case FindInPageOptions::FindInPagePrevious:
        SelectPreviousMatch();
        break;
    }
  }
}

id<CRWFindSession> FindInPageManagerImpl::GetActiveFindSession()
    API_AVAILABLE(ios(16)) {
  id<CRWFindInteraction> find_interaction = web_state_->GetFindInteraction();
  // According to the official documentation, if `findNavigatorVisible` is
  // `NO`, then `activeFindSession` should be `nil`. In practice, it is
  // necessary to check the value of `findNavigatorVisible` to ensure a Find
  // session is returned only if the Find navigator is visible.
  if (!find_interaction.findNavigatorVisible) {
    return nil;
  }
  return find_interaction.activeFindSession;
}

id<CRWFindInteraction> FindInPageManagerImpl::GetOrCreateFindInteraction()
    API_AVAILABLE(ios(16)) {
  id<CRWFindInteraction> find_interaction = web_state_->GetFindInteraction();
  if (!find_interaction) {
    web_state_->SetFindInteractionEnabled(true);
    find_interaction = web_state_->GetFindInteraction();
  }
  DCHECK(find_interaction);
  return find_interaction;
}

// Executes find logic for `FindInPageSearch` option.
void FindInPageManagerImpl::StartSearch(NSString* query)
    API_AVAILABLE(ios(16)) {
  // Stop polling Find session in case search is already ongoing.
  StopPollingActiveFindSession();

    id<CRWFindInteraction> find_interaction = GetOrCreateFindInteraction();
    // If a Find interaction should be used, prepopulate the Find navigator and
    // present it. If it is already presented, only present it again if the
    // query is different.
    if (!find_interaction.isFindNavigatorVisible ||
        ![query isEqualToString:current_query_]) {
      // For some reason, in some cases, presenting the Find navigator
      // synchronously results in inability to type in the Find navigator input
      // field. Presenting asynchronously instead solves this issue.
      dispatch_async(dispatch_get_main_queue(), ^{
        find_interaction.searchText = query;
        [find_interaction presentFindNavigatorShowingReplace:NO];
      });
    }

  // Reset latest reported Find session data.
  current_query_ = [query copy];
  current_result_count_ = -1;
  current_highlighted_result_index_ = NSNotFound;

  StartPollingActiveFindSession();
}

void FindInPageManagerImpl::SelectNextMatch() API_AVAILABLE(ios(16)) {
  [GetActiveFindSession()
      highlightNextResultInDirection:UITextStorageDirectionForward];
}

void FindInPageManagerImpl::SelectPreviousMatch() API_AVAILABLE(ios(16)) {
  [GetActiveFindSession()
      highlightNextResultInDirection:UITextStorageDirectionBackward];
}

void FindInPageManagerImpl::StopSearch() API_AVAILABLE(ios(16)) {
  StopPollingActiveFindSession();

  id<CRWFindSession> find_session = GetActiveFindSession();
  [find_session invalidateFoundResults];

    id<CRWFindInteraction> find_interaction = web_state_->GetFindInteraction();
    // If there is a Find interaction, dismiss the Find navigator. This will
    // also stop and free the active Find session stored within the Find
    // interaction.
    [find_interaction dismissFindNavigator];

    if (delegate_) {
      // Calling `DidHighlightMatches` with zero matches and no query to respond
      // to `StopFinding`.
      delegate_->DidHighlightMatches(this, web_state_, /*match_count=*/0,
                                     /*query=*/nil);
    }
}

void FindInPageManagerImpl::StopFinding() {
  if (@available(iOS 16, *)) {
    StopSearch();
  }
}

bool FindInPageManagerImpl::CanSearchContent() {
  return web_state_->IsFindInteractionSupported();
}

FindInPageManagerDelegate* FindInPageManagerImpl::GetDelegate() {
  return delegate_;
}

void FindInPageManagerImpl::SetDelegate(FindInPageManagerDelegate* delegate) {
  delegate_ = delegate;
}

void FindInPageManagerImpl::StartPollingActiveFindSession()
    API_AVAILABLE(ios(16)) {
  find_session_polling_timer_.Start(
      FROM_HERE, poll_active_find_session_delay_,
      base::BindRepeating(&FindInPageManagerImpl::PollActiveFindSession,
                          weak_factory_.GetWeakPtr()));
}

void FindInPageManagerImpl::StopPollingActiveFindSession()
    API_AVAILABLE(ios(16)) {
  find_session_polling_timer_.Stop();
}

void FindInPageManagerImpl::PollActiveFindSession() API_AVAILABLE(ios(16)) {
  id<CRWFindSession> findSession = GetActiveFindSession();
  if (!findSession) {
    // If a Find interaction is used but there is no active Find session
    // anymore, then the user dismissed the Find navigator.
    if (delegate_) {
      delegate_->UserDismissedFindNavigator(this);
    }
    StopSearch();

    return;
  }

  NSInteger new_result_count = findSession.resultCount;
  NSInteger new_highlighted_result_index = findSession.highlightedResultIndex;

  // Record FindNext/FindPrevious user actions depending on index change. This
  // can only be done if the result count is greater than 2 though, since there
  // is no way to differentiate between wrapping forward and wrapping backward
  // with 2 matches or less.
  if (new_result_count > 2) {
    // If the index increased by one or wrapped from the last match to the
    // first, then it is very likely the user tapped "Next match" or used the
    // associated keybinding.
    bool highlighted_result_index_moved_forward =
        new_highlighted_result_index == current_highlighted_result_index_ + 1 ||
        (current_highlighted_result_index_ == new_result_count - 1 &&
         new_highlighted_result_index == 0);
    if (highlighted_result_index_moved_forward) {
      RecordFindNextAction();
    }

    // If the index decreased by one or wrapped from the first match to the
    // last, then it is very likely the user tapped "Previous match" or used the
    // associated keybinding.
    bool highlighted_result_index_moved_backward =
        new_highlighted_result_index == current_highlighted_result_index_ - 1 ||
        (current_highlighted_result_index_ == 0 &&
         new_highlighted_result_index == new_result_count - 1);
    if (highlighted_result_index_moved_backward) {
      RecordFindPreviousAction();
    }
  }

  // If there are results but none is selected, select the first one.
  if (new_highlighted_result_index == NSNotFound && new_result_count > 0) {
    SelectNextMatch();
  }

  // If the result count differs from the last reported, report the new value.
  if (current_result_count_ != new_result_count) {
    if (delegate_) {
      delegate_->DidHighlightMatches(this, web_state_, new_result_count,
                                     current_query_);
    }
    current_result_count_ = new_result_count;
  }

  // If the highlighted result index differs from the last reported, report the
  // new value.
  if (current_highlighted_result_index_ != new_highlighted_result_index &&
      new_highlighted_result_index != NSNotFound) {
    if (delegate_) {
      delegate_->DidSelectMatch(this, web_state_, new_highlighted_result_index,
                                /*context_string=*/nil);
    }
    current_highlighted_result_index_ = new_highlighted_result_index;
  }
}

void FindInPageManagerImpl::WasShown(WebState* web_state) {
  if (@available(iOS 16, *)) {
    if (!GetActiveFindSession()) {
      return;
    }
    StartPollingActiveFindSession();
  }
}

void FindInPageManagerImpl::WasHidden(WebState* web_state) {
  if (@available(iOS 16, *)) {
    StopPollingActiveFindSession();
  }
}

void FindInPageManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(FindInPageManager)

}  // namespace web
