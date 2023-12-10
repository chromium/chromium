// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/intents_extension/chrome_intents_handler.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/common/intents/AddBookmarkToChromeIntent.h"
#import "ios/chrome/common/intents/AddReadingListItemToChromeIntent.h"
#import "ios/chrome/common/intents/ClearBrowsingDataIntent.h"
#import "ios/chrome/common/intents/ManagePasswordsIntent.h"
#import "ios/chrome/common/intents/ManagePaymentMethodsIntent.h"
#import "ios/chrome/common/intents/ManageSettingsIntent.h"
#import "ios/chrome/common/intents/OpenBookmarksIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/OpenLatestTabIntent.h"
#import "ios/chrome/common/intents/OpenLensIntent.h"
#import "ios/chrome/common/intents/OpenNewIncognitoTabIntent.h"
#import "ios/chrome/common/intents/OpenNewTabIntent.h"
#import "ios/chrome/common/intents/OpenReadingListIntent.h"
#import "ios/chrome/common/intents/OpenRecentTabsIntent.h"
#import "ios/chrome/common/intents/OpenTabGridIntent.h"
#import "ios/chrome/common/intents/PlayDinoGameIntent.h"
#import "ios/chrome/common/intents/RunSafetyCheckIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/common/intents/SearchWithVoiceIntent.h"
#import "ios/chrome/common/intents/SetChromeDefaultBrowserIntent.h"
#import "ios/chrome/common/intents/ViewHistoryIntent.h"

@interface ChromeIntentsHandler () <AddBookmarkToChromeIntentHandling,
                                    AddReadingListItemToChromeIntentHandling,
                                    OpenInChromeIncognitoIntentHandling,
                                    OpenInChromeIntentHandling,
                                    SearchInChromeIntentHandling,
                                    OpenReadingListIntentHandling,
                                    OpenBookmarksIntentHandling,
                                    OpenRecentTabsIntentHandling,
                                    OpenTabGridIntentHandling,
                                    SearchWithVoiceIntentHandling,
                                    OpenNewTabIntentHandling,
                                    PlayDinoGameIntentHandling,
                                    SetChromeDefaultBrowserIntentHandling,
                                    ViewHistoryIntentHandling,
                                    OpenNewIncognitoTabIntentHandling,
                                    ManagePaymentMethodsIntentHandling,
                                    RunSafetyCheckIntentHandling,
                                    ManagePasswordsIntentHandling,
                                    ManageSettingsIntentHandling,
                                    OpenLatestTabIntentHandling,
                                    OpenLensIntentHandling,
                                    ClearBrowsingDataIntentHandling>
@end

@implementation ChromeIntentsHandler

- (id)handlerForIntent:(INIntent*)intent {
  return self;
}

#pragma mark - AddBookmarkToChromeIntentHandling

- (void)resolveUrlForAddBookmarkToChrome:(AddBookmarkToChromeIntent*)intent
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

- (void)handleAddBookmarkToChrome:(AddBookmarkToChromeIntent*)intent
                       completion:(void (^)(AddBookmarkToChromeIntentResponse*))
                                      completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [AddBookmarkToChromeIntent class])];

  AddBookmarkToChromeIntentResponse* response =
      [[AddBookmarkToChromeIntentResponse alloc]
          initWithCode:AddBookmarkToChromeIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}
#pragma mark - AddReadingListItemToChromeIntentHandling

- (void)
    resolveUrlForAddReadingListItemToChrome:
        (AddReadingListItemToChromeIntent*)intent
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

- (void)
    handleAddReadingListItemToChrome:(AddReadingListItemToChromeIntent*)intent
                          completion:
                              (void (^)(
                                  AddReadingListItemToChromeIntentResponse*))
                                  completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [AddReadingListItemToChromeIntent class])];

  AddReadingListItemToChromeIntentResponse* response =
      [[AddReadingListItemToChromeIntentResponse alloc]
          initWithCode:AddReadingListItemToChromeIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
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

#pragma mark - ViewHistoryIntentHandling

- (void)handleViewHistory:(ViewHistoryIntent*)intent
               completion:(void (^)(ViewHistoryIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([ViewHistoryIntent class])];

  ViewHistoryIntentResponse* response = [[ViewHistoryIntentResponse alloc]
      initWithCode:ViewHistoryIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - OpenNewIncognitoTabIntentHandling

- (void)handleOpenNewIncognito:(OpenNewIncognitoTabIntent*)intent
                    completion:(void (^)(OpenNewIncognitoTabIntentResponse*))
                                   completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [OpenNewIncognitoTabIntent class])];

  OpenNewIncognitoTabIntentResponse* response =
      [[OpenNewIncognitoTabIntentResponse alloc]
          initWithCode:OpenNewIncognitoTabIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark - ManagePaymentMethodsIntentHandling

- (void)handleManagePaymentMethods:(ManagePaymentMethodsIntent*)intent
                        completion:
                            (void (^)(ManagePaymentMethodsIntentResponse*))
                                completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass(
                               [ManagePaymentMethodsIntent class])];

  ManagePaymentMethodsIntentResponse* response =
      [[ManagePaymentMethodsIntentResponse alloc]
          initWithCode:ManagePaymentMethodsIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark - RunSafetyCheckIntentHandling

- (void)handleRunSafetyCheck:(RunSafetyCheckIntent*)intent
                  completion:
                      (void (^)(RunSafetyCheckIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([RunSafetyCheckIntent class])];

  RunSafetyCheckIntentResponse* response = [[RunSafetyCheckIntentResponse alloc]
      initWithCode:RunSafetyCheckIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - ManagePasswordsIntentHandling

- (void)handleManagePasswords:(ManagePasswordsIntent*)intent
                   completion:
                       (void (^)(ManagePasswordsIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([ManagePasswordsIntent class])];

  ManagePasswordsIntentResponse* response =
      [[ManagePasswordsIntentResponse alloc]
          initWithCode:ManagePasswordsIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

#pragma mark - ManageSettingsIntentHandling

- (void)handleManageSettings:(ManageSettingsIntent*)intent
                  completion:
                      (void (^)(ManageSettingsIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([ManageSettingsIntent class])];

  ManageSettingsIntentResponse* response = [[ManageSettingsIntentResponse alloc]
      initWithCode:ManageSettingsIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - OpenLatestTabIntentHandling

- (void)handleOpenLatestTab:(OpenLatestTabIntent*)intent
                 completion:(void (^)(OpenLatestTabIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenLatestTabIntent class])];

  OpenLatestTabIntentResponse* response = [[OpenLatestTabIntentResponse alloc]
      initWithCode:OpenLatestTabIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - OpenLensIntentHandling

- (void)handleOpenLens:(OpenLensIntent*)intent
            completion:(void (^)(OpenLensIntentResponse*))completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([OpenLatestTabIntent class])];

  OpenLensIntentResponse* response = [[OpenLensIntentResponse alloc]
      initWithCode:OpenLensIntentResponseCodeContinueInApp
      userActivity:activity];

  completion(response);
}

#pragma mark - ClearBrowsingDataIntentHandling

- (void)handleClearBrowsingData:(ClearBrowsingDataIntent*)intent
                     completion:(void (^)(ClearBrowsingDataIntentResponse*))
                                    completion {
  NSUserActivity* activity = [[NSUserActivity alloc]
      initWithActivityType:NSStringFromClass([ClearBrowsingDataIntent class])];

  ClearBrowsingDataIntentResponse* response =
      [[ClearBrowsingDataIntentResponse alloc]
          initWithCode:ClearBrowsingDataIntentResponseCodeContinueInApp
          userActivity:activity];

  completion(response);
}

@end
