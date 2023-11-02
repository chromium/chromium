// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/refresh_control_provider.h"

#import "base/check.h"

static RefreshControlProvider* g_refreshControlProvider;

@implementation RefreshControlProvider

- (id<RemotingRefreshControl>)createForScrollView:(UIScrollView*)scrollView
                                      actionBlock:
                                          (RemotingRefreshAction)action {
  [NSException raise:@"UnimplementedException"
              format:
                  @"createRefreshControl should be overridden by the "
                  @"subclass."];
  return nil;
}

+ (void)setInstance:(RefreshControlProvider*)instance {
  DCHECK(!g_refreshControlProvider);
  g_refreshControlProvider = instance;
}

+ (RefreshControlProvider*)instance {
  DCHECK(g_refreshControlProvider);
  return g_refreshControlProvider;
}

@end
