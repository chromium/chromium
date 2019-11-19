// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_COLORS_UICOLOR_CR_DYNAMIC_COLORS_H_
#define IOS_CHROME_COMMON_COLORS_UICOLOR_CR_DYNAMIC_COLORS_H_

#import <UIKit/UIKit.h>

// This category provides convenience wrappers for the iOS 13-only functions
// dealing with dynamic colors. It can be removed along with iOS 12.
// TODO (crbug.com/981889): Remove along with iOS 12.
@interface UIColor (CRDynamicColors)

// iOS 12-compatible version of -resolvedColorWithTraitCollection.
- (UIColor*)cr_resolvedColorWithTraitCollection:
    (UITraitCollection*)traitCollection;

@end

#endif  // IOS_CHROME_COMMON_COLORS_UICOLOR_CR_DYNAMIC_COLORS_H_
