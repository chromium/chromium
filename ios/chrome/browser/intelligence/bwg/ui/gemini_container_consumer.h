// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

// Consumer protocol for updating the Gemini Container UI state.
@protocol GeminiContainerConsumer <NSObject>

// Updates the container's zero-state UI visibility.
- (void)updateZeroStateVisibility:(BOOL)visible;

// Instructs the container to dismiss any active keyboard.
- (void)dismissKeyboard;

// Sets how the actuation worklog is presented.
- (void)setWorklogDisplayMode:(ActuationWorklogDisplayMode)displayMode;

// Notifies consumer whether actuation is currently active.
- (void)setActuationActive:(BOOL)active;

// Returns the fitting height of the container's current content.
- (CGFloat)contentHeight;

// Returns the container height that shows only the actuation header.
- (CGFloat)actuationMinimizedDetentHeight;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_
