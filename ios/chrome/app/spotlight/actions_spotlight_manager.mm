// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

NSString* SpotlightActionFromString(NSString* query) {
  NSString* domain =
      [NSString stringWithFormat:@"%@.", spotlight::StringFromSpotlightDomain(
                                             spotlight::DOMAIN_ACTIONS)];
  DCHECK([query hasPrefix:domain]);
  return
      [query substringWithRange:NSMakeRange([domain length],
                                            [query length] - [domain length])];
}

}  // namespace

namespace spotlight {

// Constants for Spotlight action links.
const char kSpotlightActionNewTab[] = "OpenNewTab";
const char kSpotlightActionNewIncognitoTab[] = "OpenIncognitoTab";
const char kSpotlightActionVoiceSearch[] = "OpenVoiceSearch";
const char kSpotlightActionQRScanner[] = "OpenQRScanner";
const char kSpotlightActionSetDefaultBrowser[] = "SetDefaultBrowser";
const char kSpotlightActionLens[] = "OpenLensFromSpotlight";

// Enum is used to record the actions performed by the user.
enum {
  // Recorded when a user pressed the New Tab spotlight action.
  SPOTLIGHT_ACTION_NEW_TAB_PRESSED,
  // Recorded when a user pressed the New Incognito Tab spotlight action.
  SPOTLIGHT_ACTION_NEW_INCOGNITO_TAB_PRESSED,
  // Recorded when a user pressed the Voice Search spotlight action.
  SPOTLIGHT_ACTION_VOICE_SEARCH_PRESSED,
  // Recorded when a user pressed the QR scanner spotlight action.
  SPOTLIGHT_ACTION_QR_CODE_SCANNER_PRESSED,
  // Recorded when a user pressed the Set Default Browser spotlight action.
  SPOTLIGHT_ACTION_SET_DEFAULT_BROWSER_PRESSED,
  // Recorded when a user pressed the Lens spotlight action.
  SPOTLIGHT_ACTION_LENS_PRESSED,
  // NOTE: Add new spotlight actions in sources only immediately above this
  // line. Also, make sure the enum list for histogram `SpotlightActions` in
  // histograms.xml is updated with any change in here.
  SPOTLIGHT_ACTION_COUNT
};

// The histogram used to record user actions performed on the spotlight actions.
const char kSpotlightActionsHistogram[] = "IOS.Spotlight.Action";

BOOL SetStartupParametersForSpotlightAction(
    NSString* query,
    AppStartupParameters* startupParams) {
  DCHECK(startupParams);
  NSString* action = SpotlightActionFromString(query);
  if ([action isEqualToString:base::SysUTF8ToNSString(
                                  kSpotlightActionNewIncognitoTab)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_NEW_INCOGNITO_TAB_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [startupParams setApplicationMode:ApplicationModeForTabOpening::INCOGNITO];
  } else if ([action isEqualToString:base::SysUTF8ToNSString(
                                         kSpotlightActionVoiceSearch)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_VOICE_SEARCH_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [startupParams setApplicationMode:ApplicationModeForTabOpening::NORMAL];
    [startupParams setPostOpeningAction:START_VOICE_SEARCH];
  } else if ([action isEqualToString:base::SysUTF8ToNSString(
                                         kSpotlightActionQRScanner)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_QR_CODE_SCANNER_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [startupParams setApplicationMode:ApplicationModeForTabOpening::NORMAL];
    [startupParams setPostOpeningAction:START_QR_CODE_SCANNER];
  } else if ([action isEqualToString:base::SysUTF8ToNSString(
                                         kSpotlightActionNewTab)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_NEW_TAB_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [startupParams setApplicationMode:ApplicationModeForTabOpening::NORMAL];
  } else if ([action isEqualToString:base::SysUTF8ToNSString(
                                         kSpotlightActionSetDefaultBrowser)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_SET_DEFAULT_BROWSER_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [[UIApplication sharedApplication]
                  openURL:[NSURL
                              URLWithString:UIApplicationOpenSettingsURLString]
                  options:{}
        completionHandler:nil];
  } else if ([action isEqualToString:base::SysUTF8ToNSString(
                                         kSpotlightActionLens)]) {
    UMA_HISTOGRAM_ENUMERATION(kSpotlightActionsHistogram,
                              SPOTLIGHT_ACTION_LENS_PRESSED,
                              SPOTLIGHT_ACTION_COUNT);
    [startupParams setApplicationMode:ApplicationModeForTabOpening::NORMAL];
    [startupParams setPostOpeningAction:START_LENS_FROM_SPOTLIGHT];
  } else {
    return NO;
  }
  return YES;
}

}  // namespace spotlight

@implementation ActionsSpotlightManager

+ (ActionsSpotlightManager*)actionsSpotlightManager {
  return [[ActionsSpotlightManager alloc]
      initWithSpotlightInterface:[SpotlightInterface defaultInterface]
           searchableItemFactory:
               [[SearchableItemFactory alloc]
                   initWithLargeIconService:nil
                                     domain:spotlight::DOMAIN_ACTIONS
                      useTitleInIdentifiers:YES]];
}

#pragma mark public methods

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];
  return self;
}

- (void)indexActionsWithIsGoogleDefaultSearchEngine:
    (BOOL)isGoogleDefaultSearchEngine {
  __weak ActionsSpotlightManager* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        ActionsSpotlightManager* strongSelf = weakSelf;
        [strongSelf clearAndAddSpotlightActionsWithIsGoogleDefaultSearchEngine:
                        isGoogleDefaultSearchEngine];
      });
}

#pragma mark private methods

// Clears and re-inserts all Spotlight actions.
- (void)clearAndAddSpotlightActionsWithIsGoogleDefaultSearchEngine:
    (BOOL)isGoogleDefaultSearchEngine {
  __weak ActionsSpotlightManager* weakSelf = self;

  [self.searchableItemFactory cancelItemsGeneration];
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_ACTIONS)
      ]
                               completionHandler:^(NSError* error) {
                                 dispatch_after(
                                     dispatch_time(DISPATCH_TIME_NOW,
                                                   static_cast<int64_t>(
                                                       1 * NSEC_PER_SEC)),
                                     dispatch_get_main_queue(), ^{
                                       ActionsSpotlightManager* strongSelf =
                                           weakSelf;

                                       if (!strongSelf) {
                                         return;
                                       }

                                       [strongSelf
                                           reindexActionsToSpotlight:
                                               isGoogleDefaultSearchEngine];
                                     });
                               }];
}

- (void)reindexActionsToSpotlight:(BOOL)isGoogleDefaultSearchEngine {
  NSString* voiceSearchTitle =
      l10n_util::GetNSString(IDS_IOS_APPLICATION_SHORTCUT_VOICE_SEARCH_TITLE);
  NSString* voiceSearchAction =
      base::SysUTF8ToNSString(spotlight::kSpotlightActionVoiceSearch);

  NSString* newTabTitle =
      l10n_util::GetNSString(IDS_IOS_APPLICATION_SHORTCUT_NEWSEARCH_TITLE);
  NSString* newTabAction =
      base::SysUTF8ToNSString(spotlight::kSpotlightActionNewTab);

  NSString* incognitoTitle = l10n_util::GetNSString(
      IDS_IOS_APPLICATION_SHORTCUT_INCOGNITOSEARCH_TITLE);
  NSString* incognitoAction =
      base::SysUTF8ToNSString(spotlight::kSpotlightActionNewIncognitoTab);

  NSString* qrScannerTitle =
      l10n_util::GetNSString(IDS_IOS_APPLICATION_SHORTCUT_QR_SCANNER_TITLE);
  NSString* qrScannerAction =
      base::SysUTF8ToNSString(spotlight::kSpotlightActionQRScanner);

  NSString* defaultBrowserTitle =
      l10n_util::GetNSString(IDS_IOS_APPLICATION_SHORTCUT_SET_DEFAULT_BROWSER);
  NSString* defaultBrowserAction =
      base::SysUTF8ToNSString(spotlight::kSpotlightActionSetDefaultBrowser);

  NSMutableArray<CSSearchableItem*>* spotlightItems = [NSMutableArray array];

  [spotlightItems addObjectsFromArray:@[
    [self itemForAction:voiceSearchAction title:voiceSearchTitle],
    [self itemForAction:newTabAction title:newTabTitle],
    [self itemForAction:incognitoAction title:incognitoTitle],
    [self itemForAction:qrScannerAction title:qrScannerTitle],
    [self itemForAction:defaultBrowserAction title:defaultBrowserTitle],
  ]];

  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::Spotlight, isGoogleDefaultSearchEngine);
  if (useLens) {
    NSString* lensTitle =
        l10n_util::GetNSString(IDS_IOS_APPLICATION_SHORTCUT_LENS_TITLE);
    NSString* lensAction =
        base::SysUTF8ToNSString(spotlight::kSpotlightActionLens);
    [spotlightItems addObject:[self itemForAction:lensAction title:lensTitle]];
  }

  [self.spotlightInterface indexSearchableItems:spotlightItems];
}

// Creates a new Spotlight entry with title `title` for the given `action`.
- (CSSearchableItem*)itemForAction:(NSString*)action title:(NSString*)title {
  NSString* domainID =
      spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_ACTIONS);

  NSString* itemID = [NSString stringWithFormat:@"%@.%@", domainID, action];

  return [self.searchableItemFactory searchableItem:title
                                             itemID:itemID
                                 additionalKeywords:@[]];
}

@end
