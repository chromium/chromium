// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Track the usage of Translate over its lifetime to log metrics of whether the
// user closed the infobar without interacting with it.
typedef NS_OPTIONS(NSUInteger, UserAction) {
  UserActionNone = 0,
  UserActionTranslate = 1 << 0,
  UserActionRevert = 1 << 1,
  UserActionAlwaysTranslate = 1 << 2,
  UserActionNeverTranslateLanguage = 1 << 3,
  UserActionNeverTranslateSite = 1 << 4,
  UserActionExpandMenu = 1 << 5,
};

// UMA histogram names.
// Note: These string constants are repeated in TranslateCompactInfoBar.java.
extern const char kLanguageHistogramTranslate[];
extern const char kLanguageHistogramMoreLanguages[];
extern const char kLanguageHistogramPageNotInLanguage[];
extern const char kLanguageHistogramAlwaysTranslate[];
extern const char kLanguageHistogramNeverTranslate[];
extern const char kEventHistogram[];
extern const char kTranslationCountHistogram[];

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_CONSTANTS_H_
