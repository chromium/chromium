// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_DATA_SOURCE_H_

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

// Returns an array of feature items with entries from `whats_new_entries.plist`
// under `Features`.
NSArray<WhatsNewItem*>* WhatsNewFeatureEntries(NSString* path);

// Returns an array of chrome tip items with entries from
// `whats_new_entries.plist` under `ChromeTips`.
NSArray<WhatsNewItem*>* WhatsNewChromeTipEntries(NSString* path);

// Returns a `WhatsNewItem` from the data loaded from `whats_new_entries.plist`.
WhatsNewItem* ConstructWhatsNewItem(NSDictionary* entry);

// Returns a path to What's New entries.
NSString* WhatsNewFilePath();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_DATA_SOURCE_H_
