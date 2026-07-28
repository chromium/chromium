// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_switcher/test/test_app_switcher_helper.h"

#import <string_view>

#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"
#import "url/gurl.h"

@implementation TestAppSwitcherProviderTestHelper {
  ApplicationModeForTabOpening _mode;
}

- (void)sendAppSwitcherResponseForUrl:(const GURL&)url
                                appId:(std::string_view)appId
                           completion:
                               (AppSwitcherResponseCompletion)completion {
  if (!completion) {
    return;
  }

  ios::provider::AppSwitcherUrlOpeningResult result;
  if (url == GURL(kIncognitoModeUrl)) {
    result.is_incognito = true;
    completion(result);
    return;
  }

  if (url == GURL(kErrorUrl)) {
    result.error = [NSError errorWithDomain:@"FetchingError"
                                       code:1
                                   userInfo:nil];
    completion(result);
    return;
  }

  if (url == GURL(kTimeOutErrorUrl)) {
    result.error = [NSError errorWithDomain:@"AppSwitcherTimeoutError"
                                       code:1
                                   userInfo:nil];
    completion(result);
    return;
  }

  if (url == GURL(kAISummarizationUrl)) {
    result.is_ai_summarization = true;
    result.hashed_user_id = "test_user_hash_12345";
    completion(result);
    return;
  }

  completion(result);
}

- (void)setMode:(ApplicationModeForTabOpening)mode {
  _mode = mode;
}

- (ApplicationModeForTabOpening)mode {
  return _mode;
}

@end
