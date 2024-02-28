// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_

#import <UIKit/UIKit.h>

#import "components/tab_groups/tab_group_color.h"

// Returns a color name based on a `tab_group_color_id`.
UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
