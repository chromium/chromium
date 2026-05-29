// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/age_mismatch_learn_more/ui/age_mismatch_learn_more_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AgeMismatchLearnMoreViewController () {
  WKWebView* _webView;
  UIBarButtonItem* _backOrCloseButton;
}
@end

@implementation AgeMismatchLearnMoreViewController

- (instancetype)initWithWebView:(WKWebView*)webView {
  DCHECK(webView);
  self = [super init];
  if (self) {
    _webView = webView;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureNavigationBar];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_webView];

  [NSLayoutConstraint activateConstraints:@[
    [_webView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_webView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_webView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [_webView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

#pragma mark - Button events

// Called by the Done button from the navigation bar.
- (void)close {
  [self.delegate ageMismatchLearnMoreViewControllerWantsToBeClosed:self];
}

- (void)goBackOrClose {
  if (_webView.canGoBack) {
    [_webView goBack];
  } else {
    [self close];
  }
}

#pragma mark - Private

// Configures the top navigation bar (colors, texts, buttons, etc.)
- (void)configureNavigationBar {
  self.navigationController.navigationBar.translucent = NO;
  self.navigationController.navigationBar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.navigationController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  self.title = l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_LEARN_MORE);

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(close)];
  self.navigationItem.rightBarButtonItem = doneButton;

  _backOrCloseButton = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kChevronBackwardSymbol, 24)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(goBackOrClose)];
  _backOrCloseButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_HISTORY_BACK);

  self.navigationItem.leftBarButtonItem = _backOrCloseButton;
}

@end
