// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_

#include <Foundation/Foundation.h>

#include <string>

// The consumer protocol for the LanguageSettingsDataSource.
@protocol LanguageSettingsConsumer

// Called when the value of prefs::kOfferTranslateEnabled changes to |enabled|.
- (void)translateEnabled:(BOOL)enabled;

// Called when the value of language::prefs::kAcceptLanguages or
// language::prefs::kFluentLanguages change.
- (void)languagePrefsChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_CONSUMER_H_
