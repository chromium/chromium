// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/translate/core/common/language_detection_details.h"
#import "ios/web_view/internal/translate/cwv_translation_language_detection_details_internal.h"

@implementation CWVTranslationLanguageDetectionDetails

- (instancetype)initWithHasRunLanguageDetection:(BOOL)hasRunLanguageDetection
                                contentLanguage:(NSString*)contentLanguage
                          modelDetectedLanguage:(NSString*)modelDetectedLanguage
                                isModelReliable:(BOOL)isModelReliable
                                 hasNoTranslate:(BOOL)hasNoTranslate
                               htmlRootLanguage:(NSString*)htmlRootLanguage
                                adoptedLanguage:(NSString*)adoptedLanguage
                               reliabilityScore:(CGFloat)reliabilityScore {
  self = [super init];
  if (self) {
    _hasRunLanguageDetection = hasRunLanguageDetection;
    _contentLanguage = contentLanguage;
    _modelDetectedLanguage = modelDetectedLanguage;
    _isModelReliable = isModelReliable;
    _hasNoTranslate = hasNoTranslate;
    _htmlRootLanguage = htmlRootLanguage;
    _adoptedLanguage = adoptedLanguage;
    _reliabilityScore = reliabilityScore;
  }
  return self;
}

+ (instancetype)languageDetectionDetailsFrom:
    (const translate::LanguageDetectionDetails&)details {
  NSString* contentLanguage = base::SysUTF8ToNSString(details.content_language);
  NSString* modelDetectedLanguage =
      base::SysUTF8ToNSString(details.model_detected_language);
  NSString* htmlRootLanguage =
      base::SysUTF8ToNSString(details.html_root_language);
  NSString* adoptedLanguage = base::SysUTF8ToNSString(details.adopted_language);
  return [[CWVTranslationLanguageDetectionDetails alloc]
      initWithHasRunLanguageDetection:details.has_run_lang_detection
                      contentLanguage:contentLanguage
                modelDetectedLanguage:modelDetectedLanguage
                      isModelReliable:details.is_model_reliable
                       hasNoTranslate:details.has_notranslate
                     htmlRootLanguage:htmlRootLanguage
                      adoptedLanguage:adoptedLanguage
                     reliabilityScore:details.model_reliability_score];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[CWVTranslationLanguageDetectionDetails class]]) {
    return NO;
  }

  CWVTranslationLanguageDetectionDetails* otherDetails =
      (CWVTranslationLanguageDetectionDetails*)object;
  return (_hasRunLanguageDetection == otherDetails.hasRunLanguageDetection) &&
         [_contentLanguage isEqualToString:otherDetails.contentLanguage] &&
         [_modelDetectedLanguage
             isEqualToString:otherDetails.modelDetectedLanguage] &&
         (_isModelReliable == otherDetails.isModelReliable) &&
         (_hasNoTranslate == otherDetails.hasNoTranslate) &&
         [_htmlRootLanguage isEqualToString:otherDetails.htmlRootLanguage] &&
         [_adoptedLanguage isEqualToString:otherDetails.adoptedLanguage] &&
         (_reliabilityScore == otherDetails.reliabilityScore);
}

- (NSString*)description {
  return [NSString stringWithFormat:@"%@ adoptedLanguage:%@ hasNoTranslate:%i "
                                    @"isModelReliable:%i, reliabilityScore:%f",
                                    [super description], _adoptedLanguage,
                                    _hasNoTranslate, _isModelReliable,
                                    _reliabilityScore];
}

@end
