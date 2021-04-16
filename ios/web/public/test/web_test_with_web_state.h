// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_

#include <memory>

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#include "base/task/task_observer.h"
#include "ios/web/public/test/web_test.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class WKContentWorld;

namespace web {

class WebClient;
class WebState;

// Base test fixture that provides WebState for testing.
class WebTestWithWebState : public WebTest, public base::TaskObserver {
 public:
  // Destroys underlying WebState. web_state() will return null after this call.
  void DestroyWebState();

 protected:
  explicit WebTestWithWebState(
      WebTaskEnvironment::Options = WebTaskEnvironment::Options::DEFAULT);
  WebTestWithWebState(
      std::unique_ptr<web::WebClient> web_client,
      WebTaskEnvironment::Options = WebTaskEnvironment::Options::DEFAULT);
  ~WebTestWithWebState() override;

  // WebTest overrides.
  void SetUp() override;
  void TearDown() override;

  // Adds a pending item to the NavigationManager associated with the WebState.
  void AddPendingItem(const GURL& url, ui::PageTransition transition);

  // Loads the specified HTML content with URL into the WebState.
  void LoadHtml(NSString* html, const GURL& url);
  // Loads the specified HTML content into the WebState, using test url name.
  void LoadHtml(NSString* html);
  // Loads the specified HTML content into the WebState, using test url name.
  bool LoadHtml(const std::string& html) WARN_UNUSED_RESULT;
  // Loads the specified HTML content with URL into the WebState. None of the
  // subresources will be fetched.
  // This function is only supported on iOS11+. On iOS10, this function simply
  // calls |LoadHtml|.
  bool LoadHtmlWithoutSubresources(const std::string& html);
  // Blocks until both known NSRunLoop-based and known message-loop-based
  // background tasks have completed
  void WaitForBackgroundTasks();
  // Blocks until known NSRunLoop-based have completed, known message-loop-based
  // background tasks have completed and |condition| evaluates to true.
  void WaitForCondition(ConditionBlock condition);
  // Blocks until web_state() navigation and background tasks are
  // completed. Returns false when timed out.
  bool WaitUntilLoaded();
  // Synchronously executes JavaScript and returns result as id.
  id ExecuteJavaScript(NSString* script);
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // Synchronously executes |script| in |content_world| and returns result.
  id ExecuteJavaScript(WKContentWorld* content_world, NSString* script)
      API_AVAILABLE(ios(14.0));
#endif  // defined(__IPHONE14_0)

  // Returns the base URL of the loaded page.
  std::string BaseUrl() const;

  // Returns web state for this web controller.
  web::WebState* web_state();
  const web::WebState* web_state() const;

 private:
  // base::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // The web state for testing.
  std::unique_ptr<WebState> web_state_;
  // true if a task has been processed.
  bool processed_a_task_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
