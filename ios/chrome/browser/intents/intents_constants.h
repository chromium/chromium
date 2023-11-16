// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_INTENTS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTENTS_INTENTS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Constants for 3D touch application static shortcuts.
//
// NSUserActivity for Open New Search intent.
extern NSString* const kShortcutNewSearch;
// NSUserActivity for Open Incognito Search intent.
extern NSString* const kShortcutNewIncognitoSearch;
// NSUserActivity for Open Voice Search intent.
extern NSString* const kShortcutVoiceSearch;
// NSUserActivity for Open QR Scanner intent.
extern NSString* const kShortcutQRScanner;
// NSUserActivity for Open Lens from app icon long press intent.
extern NSString* const kShortcutLensFromAppIconLongPress;
// NSUserActivity for Open Lens from spotlight intent.
extern NSString* const kShortcutLensFromSpotlight;

// Constants for Siri shortcuts.
//
// NSUserActivity for Add Bookmark to Chrome intent.
extern NSString* const kSiriShortcutAddBookmarkToChrome;
// NSUserActivity for Add Reading List Item to Chrome intent.
extern NSString* const kSiriShortcutAddReadingListItemToChrome;
// NSUserActivity for Open in Chrome intent.
extern NSString* const kSiriShortcutOpenInChrome;
// NSUserActivity for Search in Chrome intent.
extern NSString* const kSiriShortcutSearchInChrome;
// NSUserActivity for Open in Incognito intent.
extern NSString* const kSiriShortcutOpenInIncognito;
// NSUserActivity for Open Reading List intent.
extern NSString* const kSiriOpenReadingList;
// NSUserActivity for Open Bookmarks intent.
extern NSString* const kSiriOpenBookmarks;
// NSUserActivity for Open Recent Tabs intent.
extern NSString* const kSiriOpenRecentTabs;
// NSUserActivity for Open Tab Grid intent.
extern NSString* const kSiriOpenTabGrid;
// NSUserActivity for Search with voice intent.
extern NSString* const kSiriVoiceSearch;
// NSUserActivity for Open Tab Grid intent.
extern NSString* const kSiriOpenNewTab;
// NSUserActivity for Play Dino Game intent.
extern NSString* const kSiriPlayDinoGame;
// NSUserActivity for Set Chrome as Default Browser intent.
extern NSString* const kSiriSetChromeDefaultBrowser;
// NSUserActivity for View History intent.
extern NSString* const kSiriViewHistory;
// NSUserActivity for Open new Incognito intent.
extern NSString* const kSiriOpenNewIncognitoTab;
// NSUserActivity for Manage payment Methods intent.
extern NSString* const kSiriManagePaymentMethods;
// NSUserActivity for Run Safety Check intent.
extern NSString* const kSiriRunSafetyCheck;
// NSUserActivity for Manage Passwords intent.
extern NSString* const kSiriManagePasswords;
// NSUserActivity for Manage Settings intent.
extern NSString* const kSiriManageSettings;
// NSUserActivity for Open Latest Tab intent.
extern NSString* const kSiriOpenLatestTab;
// NSUserActivity for Open Lens intent.
extern NSString* const kSiriOpenLensFromIntents;
// NSUserActivity for Clear Browsing Data intent.
extern NSString* const kSiriClearBrowsingData;

#endif  // IOS_CHROME_BROWSER_INTENTS_INTENTS_CONSTANTS_H_
