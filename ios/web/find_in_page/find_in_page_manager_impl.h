// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
#define IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#import "ios/web/find_in_page/find_in_page_request.h"
#import "ios/web/public/find_in_page/find_in_page_manager.h"
#include "ios/web/public/web_state_observer.h"

@class NSString;

namespace web {

class WebState;
class WebFrame;

class FindInPageManagerImpl : public FindInPageManager,
                              public web::WebStateObserver {
 public:
  explicit FindInPageManagerImpl(web::WebState* web_state);
  ~FindInPageManagerImpl() override;

  static void CreateForWebState(WebState* web_state);

  // FindInPageManager overrides
  void Find(NSString* query, FindInPageOptions options) override;
  void StopFinding() override;
  bool CanSearchContent() override;
  FindInPageManagerDelegate* GetDelegate() override;
  void SetDelegate(FindInPageManagerDelegate* delegate) override;

 private:
  friend class web::WebStateUserData<FindInPageManagerImpl>;

  // Executes find logic for |FindInPageSearch| option.
  void StartSearch(NSString* query);
  // Executes find logic for |FindInPageNext| option.
  void SelectNextMatch();
  // Executes find logic for |FindInPagePrevious| option.
  void SelectPreviousMatch();
  // Determines whether find is finished. If not, calls pumpSearch to
  // continue. If it is, calls UpdateFrameMatchesCount(). If find returned
  // null, then does nothing more.
  void ProcessFindInPageResult(const std::string& frame_id,
                               const int request_id,
                               const base::Value* result);
  // Calls delegate DidHighlightMatches() method if |delegate_| is set and
  // starts a FindInPageNext find. Called when the last frame returns results
  // from a Find request.
  void LastFindRequestCompleted();
  // Calls delegate DidSelectMatch() method to pass back index selected if
  // |delegate_| is set. |result| is a byproduct of using base::BindOnce() to
  // call this method after making a web_frame->CallJavaScriptFunction() call.
  void SelectDidFinish(const base::Value* result);
  // Executes highlightResult() JavaScript function in frame which contains the
  // currently selected match.
  void SelectCurrentMatch();

  // WebStateObserver overrides
  void WebFrameDidBecomeAvailable(WebState* web_state,
                                  WebFrame* web_frame) override;
  void WebFrameWillBecomeUnavailable(WebState* web_state,
                                     WebFrame* web_frame) override;
  void WebStateDestroyed(WebState* web_state) override;

  // Holds the state of the most recent find in page request.
  FindInPageRequest last_find_request_;
  FindInPageManagerDelegate* delegate_ = nullptr;
  web::WebState* web_state_ = nullptr;
  base::WeakPtrFactory<FindInPageManagerImpl> weak_factory_;
};
}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
