// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_MUTATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_MUTATOR_H_

#import <Foundation/Foundation.h>

/// Mutator for the AIM prototype composebox.
@protocol AIMPrototypeComposeboxMutator <NSObject>

/// Removes the given `item` from the context.
- (void)removeItem:(AIMInputItem*)item;

/// Sends `text` to start a query.
- (void)sendText:(NSString*)text;

/// Sets `enabled` state for AIM.
- (void)setAIModeEnabled:(BOOL)enabled;

// Attaches the current tab's content to the context.
- (void)attachCurrentTabContent;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_MUTATOR_H_
