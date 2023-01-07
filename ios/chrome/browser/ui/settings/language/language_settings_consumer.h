// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_

#include <Foundation/Foundation.h>


// The consumer protocol for the LanguageSettingsDataSource.
@protocol LanguageSettingsConsumer

// Called when the value of translate::prefs::kOfferTranslateEnabled changes to
// `enabled`.
- (void)translateEnabled:(BOOL)enabled;

// Called when the value of language::prefs::kAcceptLanguages or
// translate::prefs::kBlockedLanguages change.
- (void)languagePrefsChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_
