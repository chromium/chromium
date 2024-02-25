// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
#define IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "ios/web/public/find_in_page/find_in_page_manager.h"
#import "ios/web/public/web_state_observer.h"

#import "base/memory/weak_ptr.h"

@protocol CRWFindInteraction;
@protocol CRWFindSession;

namespace web {

class FindInPageManagerImpl : public FindInPageManager,
                              public web::WebStateObserver {
 public:
  explicit FindInPageManagerImpl(web::WebState* web_state);
  ~FindInPageManagerImpl() override;

  // AbstractFindInPageManager:
  void Find(NSString* query, FindInPageOptions options) override;
  void StopFinding() override;
  bool CanSearchContent() override;
  FindInPageManagerDelegate* GetDelegate() override;
  void SetDelegate(FindInPageManagerDelegate* delegate) override;

 private:
  friend class web::WebStateUserData<FindInPageManagerImpl>;
  friend class FindInPageManagerImplTest;

  // Lazily creates the Find interaction in the web state and returns it. Should
  // only be called if `use_find_interaction_`.
  id<CRWFindInteraction> GetOrCreateFindInteraction() API_AVAILABLE(ios(16));

  // Executes find logic for `FindInPageSearch` option.
  void StartSearch(NSString* query) API_AVAILABLE(ios(16));
  // Executes find logic for `FindInPageNext` option.
  void SelectNextMatch() API_AVAILABLE(ios(16));
  // Executes find logic for `FindInPagePrevious` option.
  void SelectPreviousMatch() API_AVAILABLE(ios(16));
  // Executes `StopFinding` logic.
  void StopSearch() API_AVAILABLE(ios(16));

  // Returns the currently active Find session if any. If there is a Find
  // interaction in the web state, then its active Find session is returned. If
  // not, `find_session_` is returned.
  id<CRWFindSession> GetActiveFindSession() API_AVAILABLE(ios(16));
  // Start calling `PollActiveFindSession` repeatedly to report Find session
  // results to `delegate_` using `find_session_polling_timer_`.
  void StartPollingActiveFindSession() API_AVAILABLE(ios(16));
  // Stop calling `PollActiveFindSession` repeatedly using
  // `find_session_polling_timer_`.
  void StopPollingActiveFindSession() API_AVAILABLE(ios(16));
  // Report the current Find session's results to `delegate_`.
  void PollActiveFindSession() API_AVAILABLE(ios(16));

  // WebStateObserver overrides
  void WasShown(WebState* web_state) override;
  void WasHidden(WebState* web_state) override;
  void WebStateDestroyed(WebState* web_state) override;

 protected:
  // Last value given to `StartSearch`. This is used in `PollActiveFindSession`
  // so a query value can be provided to the delegate.
  NSString* current_query_ = nil;
  // Last result count reported to the delegate through `DidHighlightMatches`.
  // This is used to ensure nothing is reported if the value has not changed.
  NSInteger current_result_count_ = -1;
  // Last highlighted result index reported to the delegate through
  // `DidSelectMatch`. This is used to ensure nothing is reported if the value
  // has not changed.
  NSInteger current_highlighted_result_index_ = NSNotFound;
  // Timer started in `StartPollingActiveFindSession()` and stopped in
  // `StopPollingActiveFindSession()` to periodically call
  // `PollActiveFindSession()` so as to report any changes in the state of the
  // active Find session to the delegate.
  base::RepeatingTimer find_session_polling_timer_;
  // Delay between each call to `PollActiveFindSession()`.
  base::TimeDelta poll_active_find_session_delay_;

  // Current Find session if `use_find_interaction_` is not `true`. Instantiated
  // in `StartSearch` and set back to `nil` in `StopSearch`.
  id<CRWFindSession> find_session_ API_AVAILABLE(ios(16)) = nil;

  raw_ptr<FindInPageManagerDelegate> delegate_ = nullptr;
  raw_ptr<web::WebState> web_state_ = nullptr;
  base::WeakPtrFactory<FindInPageManagerImpl> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
