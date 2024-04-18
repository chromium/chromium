// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_

#include <memory>

#import "base/ios/block_types.h"
#include "base/task/task_observer.h"
#include "ios/web/public/test/web_test.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {

class JavaScriptFeature;
class WebClient;
class WebState;

// Base test fixture that provides WebState for testing.
class WebTestWithWebState : public WebTest {
 public:
  // Destroys underlying WebState. web_state() will return null after this call.
  void DestroyWebState();

 protected:
  explicit WebTestWithWebState(
      WebTaskEnvironment::MainThreadType main_thread_type =
          WebTaskEnvironment::MainThreadType::DEFAULT);
  WebTestWithWebState(std::unique_ptr<web::WebClient> web_client,
                      WebTaskEnvironment::MainThreadType main_thread_type =
                          WebTaskEnvironment::MainThreadType::DEFAULT);
  ~WebTestWithWebState() override;

  // WebTest overrides.
  void SetUp() override;
  void TearDown() override;

  // Adds a pending item to the NavigationManager associated with the WebState.
  void AddPendingItem(const GURL& url, ui::PageTransition transition);

  // Loads the specified HTML content with URL into the WebState. Equivalent
  // to calling `LoadHtmlInWebState(html, url, web_state())`.
  void LoadHtml(NSString* html, const GURL& url);
  // Loads the specified HTML content into the WebState, using test url name.
  // Equivalent to calling `LoadHtmlInWebState(html, web_state())`.
  void LoadHtml(NSString* html);
  // Loads the specified HTML content into the WebState, using test url name.
  // Equivalent to calling `LoadHtmlInWebState(html, web_state())`.
  [[nodiscard]] bool LoadHtml(const std::string& html);
  // Loads the specified HTML content with URL into `web_state`.
  void LoadHtmlInWebState(NSString* html, const GURL& url, WebState* web_state);
  // Loads the specified HTML content into `web_state`, using test url name.
  void LoadHtmlInWebState(NSString* html, WebState* web_state);
  // Loads the specified HTML content into `web_state`, using test url name.
  [[nodiscard]] bool LoadHtmlInWebState(const std::string& html,
                                        WebState* web_state);
  // Loads the specified HTML content with URL into the WebState. None of the
  // subresources will be fetched.
  // This function is only supported on iOS11+. On iOS10, this function simply
  // calls `LoadHtml`.
  bool LoadHtmlWithoutSubresources(const std::string& html);
  // Blocks until both known NSRunLoop-based and known message-loop-based
  // background tasks have completed
  void WaitForBackgroundTasks();
  // Blocks until known NSRunLoop-based have completed, known message-loop-based
  // background tasks have completed and `condition` evaluates to true.
  [[nodiscard]] bool WaitForCondition(ConditionBlock condition);
  // Blocks until web_state() navigation and background tasks are
  // completed. Returns false when timed out.
  bool WaitUntilLoaded();
  // Synchronously returns the result of the executed JavaScript function by
  // calling `function` with `parameters` in the main frame of `web_state()`.
  std::unique_ptr<base::Value> CallJavaScriptFunction(
      const std::string& function,
      const base::Value::List& parameters);
  std::unique_ptr<base::Value> CallJavaScriptFunctionForFeature(
      const std::string& function,
      const base::Value::List& parameters,
      JavaScriptFeature* feature);
  // Synchronously executes JavaScript and returns result as id.
  virtual id ExecuteJavaScript(NSString* script);

  // Returns the base URL of the loaded page.
  std::string BaseUrl() const;

  // Returns web state for this web controller.
  web::WebState* web_state();
  const web::WebState* web_state() const;

 private:
  // The web state for testing.
  std::unique_ptr<WebState> web_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
