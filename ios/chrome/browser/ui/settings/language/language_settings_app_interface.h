// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test specific helpers for language_settings_egtest.mm.
@interface LanguageSettingsAppInterface : NSObject

// Removes all languages from "accept lang" list.
+ (void)removeAllLanguages;

// Returns comma separated "accept lang" list (f.e. @"en,fr").
+ (NSString*)languages;

// Adds language to "accept lang" list.
+ (void)addLanguage:(NSString*)language;

// Returns YES if "offer translate" setting is enabled.
+ (BOOL)offersTranslation;

// YES if user has set a preference to block the translation of |language|
// ("Never Translate This Language" option).
+ (BOOL)isBlockedLanguage:(NSString*)language;

// Simulates the last-observed translate target language. Used to determine
// which target language to offer in future.
+ (void)setRecentTargetLanguage:(NSString*)language;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_APP_INTERFACE_H_
