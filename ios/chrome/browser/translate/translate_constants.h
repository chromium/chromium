// Copyright 2019 The Chromium Authors. All rights reserved.
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

// Enum for the Translate.CompactInfobar.Event UMA histogram.
// Note: These values are repeated as constants in TranslateCompactInfoBar.java.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
// TODO(crbug.com/933371): Share these enums with Java.
enum class InfobarEvent {
  INFOBAR_IMPRESSION = 0,
  INFOBAR_TARGET_TAB_TRANSLATE = 1,
  INFOBAR_DECLINE = 2,
  INFOBAR_OPTIONS = 3,
  INFOBAR_MORE_LANGUAGES = 4,
  INFOBAR_MORE_LANGUAGES_TRANSLATE = 5,
  INFOBAR_PAGE_NOT_IN = 6,
  INFOBAR_ALWAYS_TRANSLATE = 7,
  INFOBAR_NEVER_TRANSLATE = 8,
  INFOBAR_NEVER_TRANSLATE_SITE = 9,
  INFOBAR_SCROLL_HIDE = 10,
  INFOBAR_SCROLL_SHOW = 11,
  INFOBAR_REVERT = 12,
  INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION = 13,
  INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION = 14,
  INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION = 15,
  INFOBAR_SNACKBAR_CANCEL_ALWAYS = 16,
  INFOBAR_SNACKBAR_CANCEL_NEVER_SITE = 17,
  INFOBAR_SNACKBAR_CANCEL_NEVER = 18,
  INFOBAR_ALWAYS_TRANSLATE_UNDO = 19,
  INFOBAR_CLOSE_DEPRECATED = 20,
  INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION = 21,
  INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION = 22,
  INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS = 23,
  INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER = 24,
  INFOBAR_HISTOGRAM_BOUNDARY = 25,
  kMaxValue = INFOBAR_HISTOGRAM_BOUNDARY,
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_CONSTANTS_H_
