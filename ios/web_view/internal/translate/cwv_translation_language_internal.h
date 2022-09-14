// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_INTERNAL_H_

#import "ios/web_view/public/cwv_translation_language.h"

#include <string>


NS_ASSUME_NONNULL_BEGIN

@interface CWVTranslationLanguage ()

- (instancetype)initWithLanguageCode:(const std::string&)languageCode
                       localizedName:(const std::u16string&)localizedName
                          nativeName:(const std::u16string&)nativeName
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_INTERNAL_H_
