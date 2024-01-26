// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_IMPL_H_
#define IOS_WEB_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_IMPL_H_

#include <optional>
#include <string>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#import "base/values.h"
#import "ios/web/find_in_page/java_script_find_in_page_request.h"
#import "ios/web/public/find_in_page/java_script_find_in_page_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/web_state_observer.h"

@class NSString;

namespace web {

class WebState;
class WebFrame;

class JavaScriptFindInPageManagerImpl : public JavaScriptFindInPageManager,
                                        public WebFramesManager::Observer,
                                        public WebStateObserver {
 public:
  explicit JavaScriptFindInPageManagerImpl(web::WebState* web_state);
  ~JavaScriptFindInPageManagerImpl() override;

  // FindInPageManager overrides
  void Find(NSString* query, FindInPageOptions options) override;
  void StopFinding() override;
  bool CanSearchContent() override;
  FindInPageManagerDelegate* GetDelegate() override;
  void SetDelegate(FindInPageManagerDelegate* delegate) override;

 private:
  friend class web::WebStateUserData<JavaScriptFindInPageManagerImpl>;

  // Executes find logic for `FindInPageSearch` option.
  void StartSearch(NSString* query);
  // Executes find logic for `FindInPageNext` option.
  void SelectNextMatch();
  // Executes find logic for `FindInPagePrevious` option.
  void SelectPreviousMatch();
  // Determines whether find is finished. If not, calls pumpSearch to
  // continue. If it is, calls UpdateFrameMatchesCount(). If find returned
  // null, then does nothing more.
  void ProcessFindInPageResult(const std::string& frame_id,
                               const int request_id,
                               std::optional<int> result);
  // Calls delegate DidHighlightMatches() method if `delegate_` is set and
  // starts a FindInPageNext find. Called when the last frame returns results
  // from a Find request.
  void LastFindRequestCompleted();
  // Calls delegate DidSelectMatch() method to pass back index selected if
  // `delegate_` is set. `result` is a byproduct of using base::BindOnce() to
  // call this method after making a web_frame->CallJavaScriptFunction() call.
  void SelectDidFinish(const base::Value* result);
  // Executes highlightResult() JavaScript function in frame which contains the
  // currently selected match.
  void SelectCurrentMatch();

  // WebFramesManager::Observer
  void WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                               WebFrame* web_frame) override;
  void WebFrameBecameUnavailable(WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

  // WebStateObserver overrides
  void WebStateDestroyed(WebState* web_state) override;

 protected:
  // Holds the state of the most recent find in page request.
  JavaScriptFindInPageRequest last_find_request_;
  raw_ptr<FindInPageManagerDelegate> delegate_ = nullptr;
  raw_ptr<web::WebState> web_state_ = nullptr;
  base::WeakPtrFactory<JavaScriptFindInPageManagerImpl> weak_factory_;
};
}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_IMPL_H_
