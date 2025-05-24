// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// This object corresponds to LanguageDetectionDetails defined in
// translate.mojom.
CWV_EXPORT
@interface CWVTranslationLanguageDetectionDetails : NSObject

- (instancetype)init NS_UNAVAILABLE;

// This initializer is intended only for testing.
// It should be removed once the testing API is improved.
- (instancetype)initWithHasRunLanguageDetection:(BOOL)hasRunLanguageDetection
                                contentLanguage:(NSString*)contentLanguage
                          modelDetectedLanguage:(NSString*)modelDetectedLanguage
                                isModelReliable:(BOOL)isModelReliable
                                 hasNoTranslate:(BOOL)hasNoTranslate
                               htmlRootLanguage:(NSString*)htmlRootLanguage
                                adoptedLanguage:(NSString*)adoptedLanguage
                               reliabilityScore:(CGFloat)reliabilityScore
    NS_DESIGNATED_INITIALIZER;

// Whether language detection has been run on the page.
@property(nonatomic, readonly) BOOL hasRunLanguageDetection;

// The language code detected by the content (Content-Language).
@property(nonatomic, copy, readonly) NSString* contentLanguage;

// The language code detected by the content (Content-Language).
@property(nonatomic, copy, readonly) NSString* modelDetectedLanguage;

// Whether the model detection is reliable or not.
@property(nonatomic, readonly) BOOL isModelReliable;

// Whether the notranslate is specified in head tag as a meta;
//   <meta name="google" value="notranslate"> or
//   <meta name="google" content="notranslate">.
@property(nonatomic, readonly) BOOL hasNoTranslate;

// The language code written in the lang attribute of the html element.
@property(nonatomic, copy, readonly) NSString* htmlRootLanguage;

// The adopted language code.
// This is the language code that the translation script will use as the page
// language.
@property(nonatomic, copy, readonly) NSString* adoptedLanguage;

// The reliability score of the language detection model.
@property(nonatomic, readonly) CGFloat reliabilityScore;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_H_
