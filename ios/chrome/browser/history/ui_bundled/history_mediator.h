// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

@interface HistoryMediator : NSObject<TableViewFaviconDataSource>

// The coordinator's profile.
@property(nonatomic, assign) ProfileIOS* profile;

- (instancetype)init NS_UNAVAILABLE;
// Init method. `profile` can't be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MEDIATOR_H_
