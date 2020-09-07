// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_ATTACHER_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_ATTACHER_H_

@class ViewRevealingVerticalPanHandler;
// Protocol defining an interface that sets up a thumb strip.
@protocol ThumbStripAttacher

// The thumb strip's pan gesture handler that will be added to the toolbar and
// tab strip.
@property(nonatomic, weak)
    ViewRevealingVerticalPanHandler* thumbStripPanHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_ATTACHER_H_
