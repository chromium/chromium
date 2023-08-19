// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/refresh_control_provider_chromium.h"

#import "remoting/ios/app/remoting_theme.h"

@interface RemotingRefreshControlChromium : NSObject<RemotingRefreshControl>
- (instancetype)initWithScrollView:(UIScrollView*)scrollView
                       actionBlock:(RemotingRefreshAction)actionBlock;
@end

@implementation RemotingRefreshControlChromium {
  UIRefreshControl* _refreshControl;
  RemotingRefreshAction _refreshAction;
}

- (instancetype)initWithScrollView:(UIScrollView*)scrollView
                       actionBlock:(RemotingRefreshAction)actionBlock {
  _refreshControl = [[UIRefreshControl alloc] initWithFrame:CGRectZero];
  _refreshControl.tintColor = RemotingTheme.refreshIndicatorColor;
  [scrollView addSubview:_refreshControl];
  _refreshAction = actionBlock;
  [_refreshControl addTarget:self
                      action:@selector(onRefreshTriggered)
            forControlEvents:UIControlEventValueChanged];
  return self;
}

- (BOOL)isRefreshing {
  return _refreshControl.isRefreshing;
}

- (void)endRefreshing {
  [_refreshControl endRefreshing];
}

- (void)onRefreshTriggered {
  _refreshAction();
}

@end

@implementation RefreshControlProviderChromium

- (id<RemotingRefreshControl>)createForScrollView:(UIScrollView*)scrollView
                                      actionBlock:
                                          (RemotingRefreshAction)action {
  return [[RemotingRefreshControlChromium alloc] initWithScrollView:scrollView
                                                        actionBlock:action];
}

@end
