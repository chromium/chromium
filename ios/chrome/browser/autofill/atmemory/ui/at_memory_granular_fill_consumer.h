// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;

// Consumer protocol for AtMemory granular fill UI and mediator.
@protocol AtMemoryGranularFillConsumer <NSObject>

// Sets the title for the granular fill screen.
- (void)setTitle:(NSString*)title;

// Sets the list of granular fill items to be displayed.
- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CONSUMER_H_
