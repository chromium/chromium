// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_

#import <UIKit/UIKit.h>

#include <utility>

// Returns the title and the message for the password alert from an array of
// `origins`. `first`: title and `second`: message.
std::pair<NSString*, NSString*> GetPasswordAlertTitleAndMessageForOrigins(
    NSArray<NSString*>* origins);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_
