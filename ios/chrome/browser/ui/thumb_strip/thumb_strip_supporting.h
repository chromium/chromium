// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_SUPPORTING_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_SUPPORTING_H_

#import <Foundation/Foundation.h>

@class ViewRevealingVerticalPanHandler;

// Protocol defining an interface that enables and disables the thumb strip
// on demand.
@protocol ThumbStripSupporting <NSObject>

// YES if the thumb strip is currently enabled.
@property(nonatomic, readonly, getter=isThumbStripEnabled)
    BOOL thumbStripEnabled;

// Informs that the thumb strip has been enabled, and classes that adopt this
// protocol need to do any changes necessary to support it.
- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler;

// Informs that the thumb strip has been disabled, and classes that adopt this
// protocol need to do any changes necessary.
- (void)thumbStripDisabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_SUPPORTING_H_
