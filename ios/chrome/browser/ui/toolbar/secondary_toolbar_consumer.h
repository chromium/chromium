// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_CONSUMER_H_

#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"

// Consumer protocol for the secondary toolbar.
@protocol SecondaryToolbarConsumer <ToolbarConsumer>

// Notifies the consumer to turn translucent.
- (void)makeTranslucent;

// Notifies the consumer to turn opaque.
- (void)makeOpaque;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_CONSUMER_H_
