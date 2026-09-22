// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_

#import <Foundation/Foundation.h>

#import <set>

#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {
class Time;
}  // namespace base

class GURL;

// Delegate for HistoryEntryItem. Handles actions invoked as custom
// accessibility actions.
@protocol HistoryEntryItemInterface
// Text for the content view. Rendered at the top trailing the favicon.
@property(nonatomic, copy) NSString* text;
// Detail text for content view. Rendered below text.
@property(nonatomic, copy) NSString* detailText;
// Text for the time stamp. Rendered aligned to trailing edge at same level as
// `text`.
@property(nonatomic, copy) NSString* timeText;
// URL of the associated history entry.
@property(nonatomic, assign) GURL URL;
// Timestamp of the associated history entry.
@property(nonatomic, assign) base::Time timestamp;
// Timestamps of all visits to this or similar URLs on the same day. Similar
// URLs are ones with the same title and host.
@property(nonatomic, assign) absl::flat_hash_map<GURL, std::set<base::Time>>
    allTimestamps;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_
