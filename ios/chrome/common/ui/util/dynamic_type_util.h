// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_DYNAMIC_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

// Returns an UIFont* calculated by |style| and
// min(|currentCategory|,|maxCategory|).
UIFont* PreferredFontForTextStyleWithMaxCategory(
    UIFontTextStyle style,
    UIContentSizeCategory currentCategory,
    UIContentSizeCategory maxCategory);

#endif  // IOS_CHROME_COMMON_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
