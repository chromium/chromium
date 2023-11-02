// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_REFRESH_CONTROL_PROVIDER_H_
#define REMOTING_IOS_APP_REFRESH_CONTROL_PROVIDER_H_

#import <UIKit/UIKit.h>

typedef void (^RemotingRefreshAction)();

@protocol RemotingRefreshControl<NSObject>

- (void)endRefreshing;

@property(nonatomic, readonly, getter=isRefreshing) BOOL refreshing;

@end

// A class to provide pull-to-refresh control for a scroll view, something like
// a UIRefreshControl. This class is to allow private pull-to-refresh
// implementation for the official app.
// This class will no longer be needed once MaterialComponents adds
// pull-to-refresh support:
// https://github.com/material-components/material-components-ios/issues/890
@interface RefreshControlProvider : NSObject

- (id<RemotingRefreshControl>)createForScrollView:(UIScrollView*)scrollView
                                      actionBlock:(RemotingRefreshAction)action;

@property(nonatomic, class) RefreshControlProvider* instance;

@end

#endif  // REMOTING_IOS_APP_REFRESH_CONTROL_PROVIDER_H_
