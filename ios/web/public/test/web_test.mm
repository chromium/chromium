// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "ios/web/public/deprecated/global_web_state_observer.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"

namespace web {

class WebTestRenderProcessCrashObserver : public GlobalWebStateObserver {
 public:
  WebTestRenderProcessCrashObserver() = default;
  ~WebTestRenderProcessCrashObserver() override = default;

  void RenderProcessGone(WebState* web_state) override {
    FAIL() << "Renderer process died unexpectedly during the test";
  }
};

WebTest::WebTest(WebTaskEnvironment::MainThreadType main_thread_type)
    : WebTest(std::make_unique<FakeWebClient>(), main_thread_type) {}

WebTest::WebTest(std::unique_ptr<web::WebClient> web_client,
                 WebTaskEnvironment::MainThreadType main_thread_type)
    : web_client_(std::move(web_client)),
      task_environment_(main_thread_type),
      crash_observer_(std::make_unique<WebTestRenderProcessCrashObserver>()) {}

WebTest::~WebTest() {}

void WebTest::SetUp() {
  PlatformTest::SetUp();

  DCHECK(!browser_state_);
  browser_state_ = CreateBrowserState();
  DCHECK(browser_state_);
}

std::unique_ptr<BrowserState> WebTest::CreateBrowserState() {
  return std::make_unique<FakeBrowserState>();
}

void WebTest::OverrideJavaScriptFeatures(
    std::vector<JavaScriptFeature*> features) {
  web::test::OverrideJavaScriptFeatures(GetBrowserState(), features);
}

web::WebClient* WebTest::GetWebClient() {
  return web_client_.Get();
}

BrowserState* WebTest::GetBrowserState() {
  DCHECK(browser_state_);
  return browser_state_.get();
}

void WebTest::SetIgnoreRenderProcessCrashesDuringTesting(bool allow) {
  if (allow) {
    crash_observer_ = nullptr;
  } else {
    crash_observer_ = std::make_unique<WebTestRenderProcessCrashObserver>();
  }
}

}  // namespace web
