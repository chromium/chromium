// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_PROVIDER_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_PROVIDER_WRANGLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Allows the tab grid mediator to provide information.
// TODO(crbug.com/1515084): Remove this when sync issue have been solved.
@protocol TabGridMediatorProviderWrangler

// Return the current page store in the mediator.
- (TabGridPage)currentPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_PROVIDER_WRANGLER_H_
