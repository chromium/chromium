// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_controller.h"

#import <string>

#import "base/memory/raw_ptr.h"
#import "components/content_settings/core/common/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface IncognitoViewController () <NewTabPageURLLoaderDelegate>

// The scrollview containing the actual views.
@property(nonatomic, strong) UIScrollView* incognitoView;

@end

@implementation IncognitoViewController {
  // The UrlLoadingService associated with this view.
  // TODO(crbug.com/40228520): View controllers should not have access to
  // model-layer objects. Create a mediator to connect model-layer class
  // `UrlLoadingBrowserAgent` to the view controller.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;  // weak
}

- (instancetype)initWithUrlLoader:(UrlLoadingBrowserAgent*)URLLoader {
  self = [super init];
  if (self) {
    _URLLoader = URLLoader;
  }
  return self;
}

- (void)viewDidLoad {
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

  IncognitoView* view = [[IncognitoView alloc] initWithFrame:self.view.bounds];
  view.URLLoaderDelegate = self;
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

#pragma mark - NewTabPageURLLoaderDelegate

- (void)loadURLInTab:(const GURL&)URL {
  _URLLoader->Load(UrlLoadParams::InCurrentTab(URL));
}

@end
