// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_translation_policy.h"

#import "ios/web_view/public/cwv_translation_language.h"

@interface CWVTranslationPolicy ()
// Internal initializer.
- (instancetype)initWithType:(CWVTranslationPolicyType)type
                    language:(CWVTranslationLanguage*)language
    NS_DESIGNATED_INITIALIZER;
@end

@implementation CWVTranslationPolicy

@synthesize language = _language;
@synthesize type = _type;

- (instancetype)initWithType:(CWVTranslationPolicyType)type
                    language:(CWVTranslationLanguage*)language {
  self = [super init];
  if (self) {
    _type = type;
    _language = language;
  }
  return self;
}

+ (CWVTranslationPolicy*)translationPolicyAsk {
  return [[CWVTranslationPolicy alloc] initWithType:CWVTranslationPolicyAsk
                                           language:nil];
}

+ (CWVTranslationPolicy*)translationPolicyNever {
  return [[CWVTranslationPolicy alloc] initWithType:CWVTranslationPolicyNever
                                           language:nil];
}

+ (CWVTranslationPolicy*)translationPolicyAutoTranslateToLanguage:
    (CWVTranslationLanguage*)language {
  return [[CWVTranslationPolicy alloc] initWithType:CWVTranslationPolicyAuto
                                           language:language];
}

@end
