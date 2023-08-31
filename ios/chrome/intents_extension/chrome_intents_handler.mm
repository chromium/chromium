// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/intents_extension/chrome_intents_handler.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/common/intents/OpenBookmarksIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/OpenNewTabIntent.h"
#import "ios/chrome/common/intents/OpenReadingListIntent.h"
#import "ios/chrome/common/intents/OpenRecentTabsIntent.h"
#import "ios/chrome/common/intents/OpenTabGridIntent.h"
#import "ios/chrome/common/intents/PlayDinoGameIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/common/intents/SearchWithVoiceIntent.h"
#import "ios/chrome/common/intents/SetChromeDefaultBrowserIntent.h"

@interface ChromeIntentsHandler () <OpenInChromeIncognitoIntentHandling,
                                    OpenInChromeIntentHandling,
                                    SearchInChromeIntentHandling,
                                    OpenReadingListIntentHandling,
                                    OpenBookmarksIntentHandling,
                                    OpenRecentTabsIntentHandling,
                                    OpenTabGridIntentHandling,
                                    SearchWithVoiceIntentHandling,
                                    OpenNewTabIntentHandling,
                                    PlayDinoGameIntentHandling,
                                    SetChromeDefaultBrowserIntentHandling>
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

#pragma mark - OpenReadingListIntentHandling

- (void)handleOpenReadingList:(OpenReadingListIntent*)intent
                   completion:
                       (void (^)(OpenReadingListIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenReadingListIntent class])];

  OpenReadingListIntentResponse* response =
      [[OpenReadingListIntentResponse alloc]
          initWithCode:OpenReadingListIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark - OpenBookmarksIntentHandling

- (void)handleOpenBookmarks:(OpenBookmarksIntent*)intent
                 completion:(void (^)(OpenBookmarksIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenBookmarksIntent class])];

  OpenBookmarksIntentResponse* response = [[OpenBookmarksIntentResponse alloc]
      initWithCode:OpenBookmarksIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - OpenRecentTabsIntentHandling

- (void)handleOpenRecentTabs:(OpenRecentTabsIntent*)intent
                  completion:
                      (void (^)(OpenRecentTabsIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenRecentTabsIntent class])];

  OpenRecentTabsIntentResponse* response = [[OpenRecentTabsIntentResponse alloc]
      initWithCode:OpenRecentTabsIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - OpenTabGridIntentHandling

- (void)handleOpenTabGrid:(OpenTabGridIntent*)intent
               completion:(void (^)(OpenTabGridIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenTabGridIntent class])];

  OpenTabGridIntentResponse* response = [[OpenTabGridIntentResponse alloc]
      initWithCode:OpenTabGridIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - SearchWithVoiceIntentHandling

- (void)handleSearchWithVoice:(SearchWithVoiceIntent*)intent
                   completion:
                       (void (^)(SearchWithVoiceIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([SearchWithVoiceIntent class])];

  SearchWithVoiceIntentResponse* response =
      [[SearchWithVoiceIntentResponse alloc]
          initWithCode:SearchWithVoiceIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark OpenNewTabIntentHandling

- (void)handleOpenNewTab:(SearchWithVoiceIntent*)intent
              completion:(void (^)(OpenNewTabIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenNewTabIntent class])];

  OpenNewTabIntentResponse* response = [[OpenNewTabIntentResponse alloc]
      initWithCode:OpenNewTabIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - PlayDinoGameIntentHandling

- (void)handlePlayDinoGame:(PlayDinoGameIntent*)intent
                completion:(void (^)(PlayDinoGameIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([PlayDinoGameIntent class])];

  PlayDinoGameIntentResponse* response = [[PlayDinoGameIntentResponse alloc]
      initWithCode:PlayDinoGameIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - SetChromeDefaultBrowserIntentHandling

- (void)
    handleSetChromeDefaultBrowser:(SetChromeDefaultBrowserIntent*)intent
                       completion:
                           (void (^)(SetChromeDefaultBrowserIntentResponse*))
                               completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [SetChromeDefaultBrowserIntent class])];

  SetChromeDefaultBrowserIntentResponse* response =
      [[SetChromeDefaultBrowserIntentResponse alloc]
          initWithCode:SetChromeDefaultBrowserIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

@end
