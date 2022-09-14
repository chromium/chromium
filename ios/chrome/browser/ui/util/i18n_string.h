// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_I18N_STRING_H_
#define IOS_CHROME_BROWSER_UI_UTIL_I18N_STRING_H_

#import <Foundation/Foundation.h>

// Wrapper for base::i18n::AdjustStringForLocaleDirection.
// Given the string in `text`, this function returns a new string with
// the appropriate Unicode formatting marks that mark the string direction
// (either left-to-right or right-to-left).
NSString* AdjustStringForLocaleDirection(NSString* text);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_I18N_STRING_H_
