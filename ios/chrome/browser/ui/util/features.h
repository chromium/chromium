// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_UTIL_FEATURES_H_

#import "base/feature_list.h"

// Feature flag adding support for KVO on -[UIView window].
extern const base::Feature kUIViewWindowObserving;

#endif  // IOS_CHROME_BROWSER_UI_UTIL_FEATURES_H_
