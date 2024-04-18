// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_H_

#include <memory>

#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

namespace web {

class BrowserState;
class JavaScriptFeature;
class WebClient;
class WebTestRenderProcessCrashObserver;

// A test fixture for web tests that need a minimum environment set up that
// mimics a web embedder.
class WebTest : public PlatformTest {
 protected:
  explicit WebTest(WebTaskEnvironment::MainThreadType main_thread_type =
                       WebTaskEnvironment::MainThreadType::DEFAULT);
  WebTest(std::unique_ptr<web::WebClient> web_client,
          WebTaskEnvironment::MainThreadType main_thread_type =
              WebTaskEnvironment::MainThreadType::DEFAULT);
  ~WebTest() override;

  void SetUp() override;

  // Creates and returns a BrowserState for use in tests. The default
  // implementation returns a FakeBrowserState, but subclasses can override this
  // to supply a custom BrowserState.
  virtual std::unique_ptr<BrowserState> CreateBrowserState();

  // Manually overrides the built in JavaScriptFeatures and those from
  // `GetWebClient()::GetJavaScriptFeatures()`. This is intended to be used to
  // replace an instance of a built in feature with one created by the test.
  // NOTE: Do not call this when using a ChromeWebClient or
  // `FakeWebClient::SetJavaScriptFeatures` as this will override those
  // features.
  void OverrideJavaScriptFeatures(std::vector<JavaScriptFeature*> features);

  // Returns the WebClient that is used for testing.
  virtual web::WebClient* GetWebClient();

  // Returns the BrowserState that is used for testing.
  BrowserState* GetBrowserState();

  // If called with `true`, prevents the test fixture from automatically failing
  // when a render process crashes during the test.  This is useful for tests
  // that intentionally crash the render process.  By default, the WebTest
  // fixture will fail if a render process crashes.
  void SetIgnoreRenderProcessCrashesDuringTesting(bool allow);

 private:
  // The WebClient used in tests.
  ScopedTestingWebClient web_client_;
  // The threads used for testing.
  web::WebTaskEnvironment task_environment_;
  // The browser state used in tests.
  std::unique_ptr<BrowserState> browser_state_;

  // Triggers test failures if a render process dies during the test.
  std::unique_ptr<WebTestRenderProcessCrashObserver> crash_observer_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_H_
