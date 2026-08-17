// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_

#import <Foundation/Foundation.h>

#include "base/i18n/language_tag.h"

// Commands issued to a model backing the language settings page.
@protocol LanguageSettingsCommands

// Informs the receiver to enable or disable Translate.
- (void)setTranslateEnabled:(BOOL)enabled;

// Informs the receiver to move the language with the given tag up or down in
// the list of accept languages with the given offset.
- (void)moveLanguage:(base::i18n::LanguageTag)languageTag
            downward:(BOOL)downward
          withOffset:(NSUInteger)offset;

// Informs the receiver to add the language with the given tag to the list of
// accept languages.
- (void)addLanguage:(base::i18n::LanguageTag)languageTag;

// Informs the receiver to remove the language with the given tag from the list
// of accept languages.
- (void)removeLanguage:(base::i18n::LanguageTag)languageTag;

// Informs the receiver to block the language with the given tag preventing it
// from being translated.
- (void)blockLanguage:(base::i18n::LanguageTag)languageTag;

// Informs the receiver to unblock the language with the given tag allowing
// it to be translated.
- (void)unblockLanguage:(base::i18n::LanguageTag)languageTag;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_LANGUAGE_LANGUAGE_SETTINGS_COMMANDS_H_
