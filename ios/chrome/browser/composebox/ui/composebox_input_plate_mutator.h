// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_

#import <Foundation/Foundation.h>

/// Mutator for the composebox composebox.
@protocol ComposeboxInputPlateMutator <NSObject>

/// Removes the given `item` from the context.
- (void)removeItem:(ComposeboxInputItem*)item;

/// Sends `text` to start a query.
- (void)sendText:(NSString*)text;

/// Attaches the current tab's content to the context.
- (void)attachCurrentTabContent;

/// Requests a refresh of UI.
- (void)requestUIRefresh;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_
