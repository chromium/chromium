// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"

NSString* const kSourceLanguagePrefKey = @"sourceLanguage";
NSString* const kSourceLanguageIsUnknownPrefKey = @"sourceLanguageIsUnknown";
NSString* const kTargetLanguagePrefKey = @"targetLanguage";
NSString* const kEnableTranslateButtonPrefKey = @"enableTranslateButton";
NSString* const kUpdateLanguageBeforeTranslatePrefKey =
    @"updateLanguageBeforeTranslate";
NSString* const kEnableAndDisplayShowOriginalButtonPrefKey =
    @"enableAndDisplayShowOriginalButton";
NSString* const kShouldAlwaysTranslatePrefKey = @"shouldAlwaysTranslate";
NSString* const kDisplayNeverTranslateLanguagePrefKey =
    @"displayNeverTranslateLanguage";
NSString* const kIsTranslatableLanguagePrefKey = @"isTranslatableLanguage";
NSString* const kDisplayNeverTranslateSiteButtonPrefKey =
    @"displayNeverTranslateSite";
NSString* const kIsSiteOnNeverPromptListPrefKey = @"isSiteBlacklisted";
