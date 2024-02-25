// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_

#import <Foundation/Foundation.h>
#import "base/feature_list.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

// Returns whether What's New was used in the overflow menu. This is used to
// decide on the location of the What's New entry point in the overflow menu.
bool WasWhatsNewUsed();

// Returns a string version of WhatsNewType.
const char* WhatsNewTypeToString(WhatsNewType type);

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
