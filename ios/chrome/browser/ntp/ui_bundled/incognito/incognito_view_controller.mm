// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_controller.h"

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface IncognitoViewController ()

// The scrollview containing the actual views.
@property(nonatomic, strong) UIScrollView* incognitoView;

@end

@implementation IncognitoViewController

- (void)viewDidLoad {
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

  IncognitoView* view = [[IncognitoView alloc] initWithFrame:self.view.bounds];
  view.URLLoaderDelegate = self.URLLoaderDelegate;
  self.incognitoView = view;
  self.incognitoView.accessibilityIdentifier = kNTPIncognitoViewIdentifier;
  [self.incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];
  self.incognitoView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  [self.view addSubview:self.incognitoView];
}

- (void)dealloc {
  [_incognitoView setDelegate:nil];
}

@end
