// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_CONSUMER_H_

@protocol TextZoomConsumer <NSObject>

// Tells the consumer that the user can currently zoom in.
- (void)setZoomInEnabled:(BOOL)enabled;
// Tells the consumer that the user can currently zoom out.
- (void)setZoomOutEnabled:(BOOL)enabled;
// Tells the consumer that the user can currently reset the zoom level.
- (void)setResetZoomEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_CONSUMER_H_
