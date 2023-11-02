// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents a single translatable language.
CWV_EXPORT
@interface CWVTranslationLanguage : NSObject

- (instancetype)init NS_UNAVAILABLE;

// The ISO language code. en for English, es for Spanish, etc...
// https://cloud.google.com/translate/docs/languages
@property(nonatomic, copy, readonly) NSString* languageCode;

// The language name in the current user locale. Ex: "German".
@property(nonatomic, copy, readonly) NSString* localizedName;

// The language name in the native language. Ex: "Deutsch".
@property(nonatomic, copy, readonly) NSString* nativeName;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_H_
