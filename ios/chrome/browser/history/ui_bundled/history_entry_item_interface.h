// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_

#import <Foundation/Foundation.h>

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
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_INTERFACE_H_
