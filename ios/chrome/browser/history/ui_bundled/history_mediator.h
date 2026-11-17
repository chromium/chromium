// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

class FaviconLoader;
class PrefService;
@protocol HistoryConsumer;

@interface HistoryMediator : NSObject <TableViewFaviconDataSource>

// Consumer for history UI updates.
@property(nonatomic, weak) id<HistoryConsumer> consumer;

- (instancetype)init NS_UNAVAILABLE;
// Initializes the mediator. `faviconLoader` and `prefService` must be non-null.
- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                          prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

// Disconnects from the model layer. Must be called before deallocation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_
