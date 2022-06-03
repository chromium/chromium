// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"

class ChromeBrowserState;

@interface HistoryMediator : NSObject<TableViewFaviconDataSource>

// The coordinator's BrowserState.
@property(nonatomic, assign) ChromeBrowserState* browserState;

- (instancetype)init NS_UNAVAILABLE;
// Init method. |browserState| can't be nil.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_MEDIATOR_H_
