// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/web/find_in_page/find_in_page_manager_impl.h"
#import "ios/web/find_in_page/find_in_page_metrics.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Delay between each call to `PollActiveFindSession`.
auto kPollActiveFindSessionDelay = base::Milliseconds(100);

}  // namespace

namespace web {

FindInPageManagerImpl::FindInPageManagerImpl(web::WebState* web_state,
                                             bool use_find_interaction)
    : use_find_interaction_(use_find_interaction),
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

UIFindSession* FindInPageManagerImpl::GetActiveFindSession()
    API_AVAILABLE(ios(16)) {
  // If a Find interaction should be used, then the Find session to be used is
  // the one provided by this Find interaction.
  if (use_find_interaction_) {
    UIFindInteraction* find_interaction = web_state_->GetFindInteraction();
    // According to the official documentation, if `findNavigatorVisible` is
    // `NO`, then `activeFindSession` should be `nil`. In practice, it is
    // necessary to check the value of `findNavigatorVisible` to ensure a Find
    // session is returned only if the Find navigator is visible.
    if (!find_interaction.findNavigatorVisible) {
      return nil;
    }
    return find_interaction.activeFindSession;
  }
  return find_session_;
}

UIFindInteraction* FindInPageManagerImpl::GetOrCreateFindInteraction()
    API_AVAILABLE(ios(16)) {
  DCHECK(use_find_interaction_);
  UIFindInteraction* find_interaction = web_state_->GetFindInteraction();
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
  if (!use_find_interaction_) {
    // The "IOS.FindInPage.SearchStarted" user action is associated with a new
    // text search starting i.e. when the Find UI is presented and then whenever
    // the query changes. However, if a Find interaction is used, `StartSearch`
    // will only be called when the Find panel is presented. The
    // "IOS.FindInPage.SearchStarted" user action should not be recorded in this
    // case.
    RecordSearchStartedAction();
  }

  // Stop polling Find session in case search is already ongoing.
  StopPollingActiveFindSession();

  // Reset latest reported Find session data.
  current_query_ = [query copy];
  current_result_count_ = -1;
  current_highlighted_result_index_ = NSNotFound;

  if (use_find_interaction_) {
    UIFindInteraction* find_interaction = GetOrCreateFindInteraction();
    // If a Find interaction should be used, prepopulate the Find navigator and
    // present it.
    find_interaction.searchText = query;
    [find_interaction presentFindNavigatorShowingReplace:NO];
  } else {
    if (find_session_) {
      // If a Find session already exists internally, invalidate its found
      // results.
      [find_session_ invalidateFoundResults];
    }

    web::GetWebClient()->StartTextSearchInWebState(web_state_);

    // Instantiate a new internal Find session with the given `query`.
    id<UITextSearching> searchableObject =
        web::GetWebClient()->GetSearchableObjectForWebState(web_state_);
    find_session_ = [[UITextSearchingFindSession alloc]
        initWithSearchableObject:searchableObject];
    [find_session_ performSearchWithQuery:query options:nil];
  }

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

  UIFindSession* find_session = GetActiveFindSession();
  [find_session invalidateFoundResults];

  if (use_find_interaction_) {
    UIFindInteraction* find_interaction = web_state_->GetFindInteraction();
    // If there is a Find interaction, dismiss the Find navigator. This will
    // also stop and free the active Find session stored within the Find
    // interaction.
    [find_interaction dismissFindNavigator];
  } else {
    find_session_ = nil;
    web::GetWebClient()->StopTextSearchInWebState(web_state_);
  }

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
      FROM_HERE, kPollActiveFindSessionDelay,
      base::BindRepeating(&FindInPageManagerImpl::PollActiveFindSession,
                          weak_factory_.GetWeakPtr()));
}

void FindInPageManagerImpl::StopPollingActiveFindSession()
    API_AVAILABLE(ios(16)) {
  find_session_polling_timer_.Stop();
}

void FindInPageManagerImpl::PollActiveFindSession() API_AVAILABLE(ios(16)) {
  UIFindSession* findSession = GetActiveFindSession();
  if (!findSession || !delegate_) {
    if (use_find_interaction_) {
      // If a Find interaction is used but there is no active Find session
      // anymore, then the user dismissed the Find navigator.
      delegate_->UserDismissedFindNavigator(this);
      StopSearch();
    } else {
      StopPollingActiveFindSession();
    }

    return;
  }

  NSInteger new_result_count = findSession.resultCount;
  NSInteger new_highlighted_result_index = findSession.highlightedResultIndex;

  // If the index increased by one or wrapped from the last match to the first,
  // then it is very likely the user tapped "Next match" or used the associated
  // keybinding.
  if (new_highlighted_result_index == current_highlighted_result_index_ + 1 ||
      (current_highlighted_result_index_ == new_result_count - 1 &&
       new_highlighted_result_index == 0)) {
    RecordFindNextAction();
  }

  // If the index decreased by one or wrapped from the first match to the last,
  // then it is very likely the user tapped "Previous match" or used the
  // associated keybinding.
  if (new_highlighted_result_index == current_highlighted_result_index_ - 1 ||
      (current_highlighted_result_index_ == 0 &&
       new_highlighted_result_index == new_result_count - 1)) {
    RecordFindPreviousAction();
  }

  // If there are results but none is selected, select the first one.
  if (new_highlighted_result_index == NSNotFound && new_result_count > 0) {
    SelectNextMatch();
  }

  // If the result count differs from the last reported, report the new value.
  if (current_result_count_ != new_result_count) {
    delegate_->DidHighlightMatches(this, web_state_, new_result_count,
                                   current_query_);
    current_result_count_ = new_result_count;
  }

  // If the highlighted result index differs from the last reported, report the
  // new value.
  if (current_highlighted_result_index_ != new_highlighted_result_index &&
      new_highlighted_result_index != NSNotFound) {
    delegate_->DidSelectMatch(this, web_state_, new_highlighted_result_index,
                              /*context_string=*/nil);
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
