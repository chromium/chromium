// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace {
// Pref keys passed through setupModalViewControllerWithPrefs:.
NSString* kSourceLanguagePrefKey = @"sourceLanguage";
NSString* kSourceLanguageIsUnknownPrefKey = @"sourceLanguageIsUnknown";
NSString* kTargetLanguagePrefKey = @"targetLanguage";
NSString* kEnableTranslateButtonPrefKey = @"enableTranslateButton";
NSString* kUpdateLanguageBeforeTranslatePrefKey =
    @"updateLanguageBeforeTranslate";
NSString* kEnableAndDisplayShowOriginalButtonPrefKey =
    @"enableAndDisplayShowOriginalButton";
NSString* kShouldAlwaysTranslatePrefKey = @"shouldAlwaysTranslate";
NSString* kDisplayNeverTranslateLanguagePrefKey =
    @"displayNeverTranslateLanguage";
NSString* kIsTranslatableLanguagePrefKey = @"isTranslatableLanguage";
NSString* kDisplayNeverTranslateSiteButtonPrefKey =
    @"displayNeverTranslateSite";
NSString* kIsSiteOnNeverPromptListPrefKey = @"isSiteBlacklisted";
}

// Protocol consumer used to push information to the Infobar Translate Modal UI
// for it to properly configure itself.
@protocol InfobarTranslateModalConsumer

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:
    (NSDictionary<NSString*, NSObject*>*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
