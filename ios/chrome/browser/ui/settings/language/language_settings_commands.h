// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_

#include <Foundation/Foundation.h>

#include <string>

// Commands issued to a model backing the language settings page.
@protocol LanguageSettingsCommands

// Informs the receiver to enable or disable Translate.
- (void)setTranslateEnabled:(BOOL)enabled;

// Informs the receiver to move the language with the given code up or down in
// the list of accept languages with the given offset.
- (void)moveLanguage:(const std::string&)languageCode
            downward:(BOOL)downward
          withOffset:(NSUInteger)offset;

// Informs the receiver to add the language with the given code to the list of
// accept languages.
- (void)addLanguage:(const std::string&)languageCode;

// Informs the receiver to remove the language with the given code from the list
// of accept languages.
- (void)removeLanguage:(const std::string&)languageCode;

// Informs the receiver to block the language with the given code preventing it
// from being translated.
- (void)blockLanguage:(const std::string&)languageCode;

// Informs the receiver to unblock the language with the given code allowing
// it to be translated.
- (void)unblockLanguage:(const std::string&)languageCode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_
