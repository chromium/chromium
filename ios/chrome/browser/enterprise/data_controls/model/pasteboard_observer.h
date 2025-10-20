// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_PASTEBOARD_OBSERVER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_PASTEBOARD_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"

@class UIPasteboard;

// An object that observes pasteboard changes by subscribing to
// `UIPasteboardChangedNotification` and by checking the pasteboard's change
// count when the app becomes active.
@interface PasteboardObserver : NSObject

// The designated initializer. `callback` will be invoked when a pasteboard
// change is detected, either via `UIPasteboardChangedNotification` or when the
// app becomes active with a different pasteboard change count.
- (instancetype)initWithCallback:
    (base::RepeatingCallback<void(UIPasteboard*)>)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_PASTEBOARD_OBSERVER_H_
