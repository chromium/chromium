// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_test.h"

#include "base/memory/ptr_util.h"
#include "ios/web/public/deprecated/global_web_state_observer.h"
#import "ios/web/public/test/fakes/test_web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebTestRenderProcessCrashObserver : public GlobalWebStateObserver {
 public:
  WebTestRenderProcessCrashObserver() = default;
  ~WebTestRenderProcessCrashObserver() override = default;

  void RenderProcessGone(WebState* web_state) override {
    FAIL() << "Renderer process died unexpectedly during the test";
  }
};

WebTest::WebTest(WebTaskEnvironment::Options options)
    : WebTest(base::WrapUnique(new TestWebClient), options) {}

WebTest::WebTest(std::unique_ptr<web::WebClient> web_client,
                 WebTaskEnvironment::Options options)
    : web_client_(std::move(web_client)),
      task_environment_(options),
      crash_observer_(std::make_unique<WebTestRenderProcessCrashObserver>()) {}

WebTest::~WebTest() {}

web::WebClient* WebTest::GetWebClient() {
  return web_client_.Get();
}

BrowserState* WebTest::GetBrowserState() {
  return &browser_state_;
}

void WebTest::SetIgnoreRenderProcessCrashesDuringTesting(bool allow) {
  if (allow) {
    crash_observer_ = nullptr;
  } else {
    crash_observer_ = std::make_unique<WebTestRenderProcessCrashObserver>();
  }
}

void WebTest::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  browser_state_.SetSharedURLLoaderFactory(
      std::move(shared_url_loader_factory));
}

}  // namespace web
