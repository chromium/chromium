// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"

#include <string>

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/ntp/incognito_view.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoViewController ()
// The scrollview containing the actual views.
@property(nonatomic, strong) IncognitoView* incognitoView;
@property(nonatomic, weak) id<UrlLoader> loader;
@end

@implementation IncognitoViewController

@synthesize incognitoView = _incognitoView;
@synthesize loader = _loader;

- (id)initWithLoader:(id<UrlLoader>)loader {
  self = [super init];
  if (self) {
    _loader = loader;
  }
  return self;
}

- (void)viewDidLoad {
  self.incognitoView = [[IncognitoView alloc]
      initWithFrame:[UIApplication sharedApplication].keyWindow.bounds
          urlLoader:self.loader];
  [self.incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];

  [self.incognitoView
      setBackgroundColor:[UIColor colorWithWhite:34 / 255.0 alpha:1.0]];

  [self.view addSubview:self.incognitoView];
}

- (void)dealloc {
  [_incognitoView setDelegate:nil];
}

#pragma mark - CRWNativeContent

- (void)wasShown {
}

- (void)reload {
}

- (void)wasHidden {
}

- (CGPoint)scrollOffset {
  return CGPointZero;
}

- (void)dismissModals {
}

- (void)willUpdateSnapshot {
}

- (const GURL&)url {
  return GURL::EmptyGURL();
}

- (BOOL)isViewAlive {
  return YES;
}

@end
