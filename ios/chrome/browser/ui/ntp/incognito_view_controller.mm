// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"

#include <string>

#include "components/content_settings/core/common/features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/ntp/incognito_view.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoViewController ()

// The scrollview containing the actual views.
@property(nonatomic, strong) IncognitoView* incognitoView;

@end

@implementation IncognitoViewController {
  // The UrlLoadingService associated with this view.
  UrlLoadingBrowserAgent* _URLLoader;  // weak
}

- (instancetype)initWithUrlLoader:(UrlLoadingBrowserAgent*)URLLoader {
  self = [super init];
  if (self) {
    _URLLoader = URLLoader;
  }
  return self;
}

- (void)viewDidLoad {
  if (@available(iOS 13, *)) {
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }

  self.incognitoView = [[IncognitoView alloc] initWithFrame:self.view.bounds
                                                  URLLoader:_URLLoader];
  [self.incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];
  UIColor* backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.incognitoView.backgroundColor = backgroundColor;
  [self.view addSubview:self.incognitoView];
}

- (void)dealloc {
  [_incognitoView setDelegate:nil];
}

@end
