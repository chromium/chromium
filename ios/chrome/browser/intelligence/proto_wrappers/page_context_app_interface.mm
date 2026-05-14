// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_app_interface.h"

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

const base::TimeDelta kApcFetchingTimeout = base::Seconds(10);

@implementation PageContextAppInterface

+ (NSData*)fetchLatestAPCWithRichExtraction:(BOOL)useRichExtraction
                             actionableMode:(BOOL)useActionableMode {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState) {
    return nil;
  }

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRichExtraction(useRichExtraction)
          .SetUseRichExtractionWithActionable(useActionableMode)
          .Build();

  __block NSData* resultData = nil;
  __block BOOL completed = NO;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:webState
                  config:config
      completionCallback:base::BindOnce(^(
                             PageContextWrapperCallbackResponse response) {
        if (response.has_value()) {
          std::string serialized;
          response.value()->SerializeToString(&serialized);
          resultData = [NSData dataWithBytes:serialized.data()
                                      length:serialized.length()];
        }
        completed = YES;
      })];
  wrapper.shouldGetAnnotatedPageContent = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:kApcFetchingTimeout];

  bool success =
      base::test::ios::WaitUntilConditionOrTimeout(kApcFetchingTimeout, ^bool {
        return completed;
      });
  if (!success) {
    return nil;
  }
  return resultData;
}

@end
