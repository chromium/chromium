// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_global_state_util.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "ios/web/public/init/web_main.h"
#import "ios/web_view/internal/web_view_web_client.h"
#import "ios/web_view/internal/web_view_web_main_delegate.h"
#import "testing/coverage_util_ios.h"

namespace ios_web_view {

void InitializeGlobalState() {
  static std::unique_ptr<ios_web_view::WebViewWebClient> web_client;
  static std::unique_ptr<ios_web_view::WebViewWebMainDelegate>
      web_main_delegate;
  static std::unique_ptr<web::WebMain> web_main;
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // This is for generating coverage data for tests only.
    coverage_util::ConfigureCoverageReportPath();
    web_client = std::make_unique<ios_web_view::WebViewWebClient>();
    web::SetWebClient(web_client.get());

    web_main_delegate =
        std::make_unique<ios_web_view::WebViewWebMainDelegate>();
    web::WebMainParams params(web_main_delegate.get());
    web_main = std::make_unique<web::WebMain>(std::move(params));

    [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationWillTerminateNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* _Nonnull note) {
                  // These global variables should be destructed when the app is
                  // about to terminate, and in reverse order to construction.
                  web_main.reset();
                  web_main_delegate.reset();
                  web_client.reset();
                }];
  });
}

}  // namespace ios_web_view
