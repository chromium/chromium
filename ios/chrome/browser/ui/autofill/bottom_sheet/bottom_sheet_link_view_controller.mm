// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_view_controller.h"

#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_view_controller_presentation_delegate.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

@implementation BottomSheetLinkViewController {
  std::unique_ptr<web::WebState> _webState;
  CrURL* _url;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                               title:(NSString*)title {
  self = [super init];
  if (self) {
    web::WebState::CreateParams params(browserState);
    _webState = web::WebState::Create(params);
    AttachTabHelpers(_webState.get(), /*for_prerender=*/false);
    self.title = title;
  }
  return self;
}

- (void)loadView {
  [super loadView];
  UIView* webView = _webState->GetView();
  [self.view addSubview:webView];

  [NSLayoutConstraint activateConstraints:@[
    [webView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [webView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [webView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [webView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

- (void)viewDidLoad {
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDoneButton)];
}

- (void)viewDidAppear:(BOOL)animated {
  _webState->WasShown();
}

- (void)viewDidDisappear:(BOOL)animated {
  _webState->WasHidden();
}

- (void)openURL:(CrURL*)url {
  web::NavigationManager::WebLoadParams webLoadParams(url.gurl);
  _webState->GetNavigationManager()->LoadURLWithParams(webLoadParams);
}

#pragma mark - Private

// Handles tapping the done button.
- (void)didTapDoneButton {
  [self.presentationDelegate dismissBottomSheetLinkView];
}

@end
