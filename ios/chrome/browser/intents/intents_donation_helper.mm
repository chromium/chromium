// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/intents_donation_helper.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/intents/ClearBrowsingDataIntent.h"
#import "ios/chrome/common/intents/OpenBookmarksIntent.h"
#import "ios/chrome/common/intents/OpenLatestTabIntent.h"
#import "ios/chrome/common/intents/OpenLensIntent.h"
#import "ios/chrome/common/intents/OpenNewTabIntent.h"
#import "ios/chrome/common/intents/OpenReadingListIntent.h"
#import "ios/chrome/common/intents/OpenRecentTabsIntent.h"
#import "ios/chrome/common/intents/OpenTabGridIntent.h"
#import "ios/chrome/common/intents/PlayDinoGameIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/common/intents/SearchWithVoiceIntent.h"
#import "ios/chrome/common/intents/SetChromeDefaultBrowserIntent.h"
#import "ios/chrome/common/intents/ViewHistoryIntent.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation IntentDonationHelper

+ (void)donateIntent:(IntentType)intentType {
  base::UmaHistogramEnumeration("IOS.Spotlight.DonatedIntentType", intentType);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        INInteraction* interaction = [self interactionForIntentType:intentType];
        [interaction donateInteractionWithCompletion:nil];
      }));
}

+ (INInteraction*)interactionForIntentType:(IntentType)intentType {
  switch (intentType) {
    case IntentType::kSearchInChrome: {
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
    case IntentType::kOpenReadingList: {
      OpenReadingListIntent* openReadingListIntent =
          [[OpenReadingListIntent alloc] init];
      openReadingListIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_READING_LIST_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openReadingListIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kOpenBookmarks: {
      OpenBookmarksIntent* openBookmarksIntent =
          [[OpenBookmarksIntent alloc] init];
      openBookmarksIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_BOOKMARKS_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openBookmarksIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kOpenRecentTabs: {
      OpenRecentTabsIntent* openRecentTabsIntent =
          [[OpenRecentTabsIntent alloc] init];
      openRecentTabsIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_RECENT_TABS_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openRecentTabsIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kOpenTabGrid: {
      OpenTabGridIntent* openTabGridIntent = [[OpenTabGridIntent alloc] init];
      openTabGridIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_TAB_GRID_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openTabGridIntent response:nil];
      return interaction;
    }
    case IntentType::kOpenVoiceSearch: {
      SearchWithVoiceIntent* searchWithVoiceIntent =
          [[SearchWithVoiceIntent alloc] init];
      searchWithVoiceIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_SEARCH_WITH_VOICE_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:searchWithVoiceIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kOpenNewTab: {
      OpenNewTabIntent* openNewTabIntent = [[OpenNewTabIntent alloc] init];
      openNewTabIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_NEW_TAB_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:openNewTabIntent response:nil];
      return interaction;
    }
    case IntentType::kPlayDinoGame: {
      PlayDinoGameIntent* playDinoGameIntent =
          [[PlayDinoGameIntent alloc] init];
      playDinoGameIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_PLAY_DINO_GAME_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:playDinoGameIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kSetDefaultBrowser: {
      SetChromeDefaultBrowserIntent* setChromeDefaultBrowserIntent =
          [[SetChromeDefaultBrowserIntent alloc] init];
      setChromeDefaultBrowserIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(
              IDS_IOS_INTENTS_SET_CHROME_DEFAULT_BROWSER_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:setChromeDefaultBrowserIntent
                                       response:nil];
      return interaction;
    }
    case IntentType::kViewHistory: {
      ViewHistoryIntent* viewHistoryIntent = [[ViewHistoryIntent alloc] init];
      viewHistoryIntent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_VIEW_CHROME_HISTORY_TITLE);
      INInteraction* interaction =
          [[INInteraction alloc] initWithIntent:viewHistoryIntent response:nil];
      return interaction;
    }
    case IntentType::kOpenLatestTab: {
      OpenLatestTabIntent* intent = [[OpenLatestTabIntent alloc] init];
      intent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_LATEST_TAB_TITLE);
      INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                                response:nil];
      return interaction;
    }
    case IntentType::kStartLens: {
      OpenLensIntent* intent = [[OpenLensIntent alloc] init];
      intent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_OPEN_LENS_TITLE);
      INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                                response:nil];
      return interaction;
    }
    case IntentType::kClearBrowsingData: {
      ClearBrowsingDataIntent* intent = [[ClearBrowsingDataIntent alloc] init];
      intent.suggestedInvocationPhrase =
          l10n_util::GetNSString(IDS_IOS_INTENTS_CLEAR_BROWSING_DATA_TITLE);
      INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                                response:nil];
      return interaction;
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      return nil;
    }
  }
}

@end
