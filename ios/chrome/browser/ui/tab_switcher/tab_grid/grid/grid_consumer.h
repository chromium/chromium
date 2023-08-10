// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_

// Consumer for model to push configurations to the grid UI.
@protocol GridConsumer

// YES if there is items that can be restored.
- (void)setItemsCanBeRestored:(BOOL)itemsCanBeRestored;

// YES if there is items that can be close.
- (void)setItemsCanBeClosed:(BOOL)itemsCanBeClosed;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_
