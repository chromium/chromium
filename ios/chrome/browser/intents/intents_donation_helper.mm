// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/intents_donation_helper.h"

#import "base/notreached.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/common/intents/OpenBookmarksIntent.h"
#import "ios/chrome/common/intents/OpenReadingListIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation IntentDonationHelper

+ (void)donateIntent:(IntentType)intentType {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        INInteraction* interaction = [self interactionForIntentType:intentType];
        [interaction donateInteractionWithCompletion:nil];
      }));
}

+ (INInteraction*)interactionForIntentType:(IntentType)intentType {
  switch (intentType) {
    case INTENT_SEARCH_IN_CHROME: {
      SearchInChromeIntent* searchInChromeIntent =
          [[SearchInChromeIntent alloc] init];

      // SiriKit requires the intent parameter to be set to a non-empty
      // string in order to accept the intent donation. Set it to a single
      // space, to be later trimmed by the intent handler, which will result
      // in the shortcut being treated as if no search phrase was supplied.
      searchInChromeIntent.searchPhrase = @" ";
      searchInChromeIntent.suggestedInvocationPhrase = l10n_util::GetNSString(
          IDS_IOS_INTENTS_SEARCH_IN_CHROME_INVOCATION_PHRASE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:searchInChromeIntent
                                       response:nil];
      return interaction;
    }
    case INTENT_OPEN_READING_LIST: {
      OpenReadingListIntent* openReadingListIntent =
          [[OpenReadingListIntent alloc] init];
      openReadingListIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_READING_LIST_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openReadingListIntent
                                       response:nil];
      return interaction;
    }
    case INTENT_OPEN_BOOKMARKS: {
      OpenBookmarksIntent* openBookmarksIntent =
          [[OpenBookmarksIntent alloc] init];
      openBookmarksIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_BOOKMARKS_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openBookmarksIntent
                                       response:nil];
      return interaction;
    }
    default: {
      NOTREACHED();
      return nil;
    }
  }
}

@end
