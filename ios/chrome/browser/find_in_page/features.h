// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FEATURES_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FEATURES_H_

#import "base/feature_list.h"

// Feature flag that enables Native Find in Page.
BASE_DECLARE_FEATURE(kNativeFindInPage);

// Feature parameters for Native Find in Page. If no parameter is set, Find in
// Page will use the system Find panel.
extern const char kNativeFindInPageParameterName[];

// Indicates if Native Find in Page with Chrome Find bar should be used.
extern const char kNativeFindInPageWithChromeFindBarParam[];

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FEATURES_H_
