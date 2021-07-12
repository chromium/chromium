// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DiscoverFeedPreviewViewController ()

// The view of the loaded webState.
@property(nonatomic, strong) UIView* webStateView;

@end

@implementation DiscoverFeedPreviewViewController

- (instancetype)initWithView:(UIView*)webStateView {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _webStateView = webStateView;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.webStateView setFrame:[self.view bounds]];
  self.webStateView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:self.webStateView];
}

@end
