// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

// Pref keys passed through setupModalViewControllerWithPrefs:.
extern NSString* const kSourceLanguagePrefKey;
extern NSString* const kSourceLanguageIsUnknownPrefKey;
extern NSString* const kTargetLanguagePrefKey;
extern NSString* const kEnableTranslateButtonPrefKey;
extern NSString* const kUpdateLanguageBeforeTranslatePrefKey;
extern NSString* const kEnableAndDisplayShowOriginalButtonPrefKey;
extern NSString* const kShouldAlwaysTranslatePrefKey;
extern NSString* const kDisplayNeverTranslateLanguagePrefKey;
extern NSString* const kIsTranslatableLanguagePrefKey;
extern NSString* const kDisplayNeverTranslateSiteButtonPrefKey;
extern NSString* const kIsSiteOnNeverPromptListPrefKey;

// Protocol consumer used to push information to the Infobar Translate Modal UI
// for it to properly configure itself.
@protocol InfobarTranslateModalConsumer

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:
    (NSDictionary<NSString*, NSObject*>*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
