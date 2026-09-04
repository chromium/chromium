// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_MUTATOR_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;

// Mutator protocol for user actions in the AtMemory granular fill UI.
@protocol AtMemoryGranularFillMutator <NSObject>

// Called when the user taps to fill `item`.
- (void)didSelectGranularFillItem:(AtMemoryGranularFillItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_MUTATOR_H_
