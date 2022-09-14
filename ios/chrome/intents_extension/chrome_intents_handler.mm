// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/intents_extension/chrome_intents_handler.h"

#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeIntentsHandler () <OpenInChromeIncognitoIntentHandling,
                                    OpenInChromeIntentHandling,
                                    SearchInChromeIntentHandling>
@end

@implementation ChromeIntentsHandler

- (id)handlerForIntent:(INIntent*)intent {
  return self;
}

#pragma mark - OpenInChromeIncognitoIntentHandling

- (void)resolveUrlForOpenInChromeIncognito:(OpenInChromeIncognitoIntent*)intent
                            withCompletion:
                                (void (^)(NSArray<INURLResolutionResult*>*))
                                    completion {
  NSMutableArray<INURLResolutionResult*>* result =
      [NSMutableArray arrayWithCapacity:intent.url.count];

  for (NSURL* url in intent.url) {
    [result addObject:[INURLResolutionResult successWithResolvedURL:url]];
  }

  completion(result);
}

- (void)handleOpenInChromeIncognito:(OpenInChromeIncognitoIntent*)intent
                         completion:
                             (void (^)(OpenInChromeIncognitoIntentResponse*))
                                 completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [OpenInChromeIncognitoIntent class])];

  OpenInChromeIncognitoIntentResponse* response =
      [[OpenInChromeIncognitoIntentResponse alloc]
          initWithCode:OpenInChromeIncognitoIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark - OpenInChromeIntentHandling

- (void)resolveUrlForOpenInChrome:(OpenInChromeIntent*)intent
                   withCompletion:
                       (void (^)(NSArray<INURLResolutionResult*>*))completion {
  NSMutableArray<INURLResolutionResult*>* result =
      [NSMutableArray arrayWithCapacity:intent.url.count];

  for (NSURL* url in intent.url) {
    [result addObject:[INURLResolutionResult successWithResolvedURL:url]];
  }

  completion(result);
}

- (void)handleOpenInChrome:(OpenInChromeIntent*)intent
                completion:
                    (void (^)(OpenInChromeIntentResponse* response))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenInChromeIntent class])];

  OpenInChromeIntentResponse* response = [[OpenInChromeIntentResponse alloc]
      initWithCode:OpenInChromeIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - SearchInChromeIntentHandling

- (void)resolveSearchPhraseForSearchInChrome:(SearchInChromeIntent*)intent
                              withCompletion:
                                  (void (^)(INStringResolutionResult*))
                                      completion {
  INStringResolutionResult* result =
      [INStringResolutionResult successWithResolvedString:intent.searchPhrase];

  completion(result);
}

- (void)handleSearchInChrome:(SearchInChromeIntent*)intent
                  completion:
                      (void (^)(SearchInChromeIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [SearchInChromeIntentResponse class])];

  SearchInChromeIntentResponse* response = [[SearchInChromeIntentResponse alloc]
      initWithCode:SearchInChromeIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

@end
