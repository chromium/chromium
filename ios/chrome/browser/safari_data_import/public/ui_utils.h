// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UI_UTILS_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UI_UTILS_H_

#import <UIKit/UIKit.h>

/// Determines which font text style to use depending on the device size, the
/// size class and if dynamic type is enabled.
UIFontTextStyle GetSafariDataImportTitleLabelFontTextStyle(
    UITraitCollection* traitCollection);

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UI_UTILS_H_
