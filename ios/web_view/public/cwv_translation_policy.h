// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_POLICY_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_POLICY_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationLanguage;

typedef NS_ENUM(NSInteger, CWVTranslationPolicyType) {
  CWVTranslationPolicyAsk = 1,  // Prompt user on whether or not to translate.
  CWVTranslationPolicyNever,    // Never translate.
  CWVTranslationPolicyAuto      // Automatically translate according to policy.
};

// Represents a translation policy that can associated with another object like
// a language or website hostname.
CWV_EXPORT
@interface CWVTranslationPolicy : NSObject

// Creates a policy with CWVTranslationPolicyAsk and null language.
+ (CWVTranslationPolicy*)translationPolicyAsk;
// Creates a policy with CWVTranslationPolicyNever and null language.
+ (CWVTranslationPolicy*)translationPolicyNever;
// Creates a policy with CWVTranslationPolicyAuto and the given language.
+ (CWVTranslationPolicy*)translationPolicyAutoTranslateToLanguage:
    (CWVTranslationLanguage*)language;

// Policy type.
@property(nonatomic, readonly) CWVTranslationPolicyType type;

// Indicates the target language to automatically translate to.
// It is nil unless |type| is CWVTranslationPolicyAuto.
@property(nonatomic, nullable, readonly) CWVTranslationLanguage* language;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_POLICY_H_
