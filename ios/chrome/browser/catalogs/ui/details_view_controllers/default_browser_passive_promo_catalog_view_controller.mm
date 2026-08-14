// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/details_view_controllers/default_browser_passive_promo_catalog_view_controller.h"

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_view.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
constexpr CGFloat kVerticalMargin = 20;
constexpr CGFloat kHorizontalMargin = 16;
}  // namespace

@implementation DefaultBrowserPassivePromoCatalogViewController {
  DefaultBrowserPassivePromoCardView* _promoView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"Passive Default Browser Promo";
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self setupViews];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if ([self.navigationController respondsToSelector:@selector(closeSettings)]) {
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self.navigationController
                             action:@selector(closeSettings)];
    self.navigationItem.rightBarButtonItem = doneButton;
  }
}

#pragma mark - Private

- (void)setupViews {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  _promoView =
      [[DefaultBrowserPassivePromoCardView alloc] initWithFrame:CGRectZero];
  [scrollView addSubview:_promoView];

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [_promoView.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor
                       constant:kVerticalMargin],
    [_promoView.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor
                       constant:-kVerticalMargin],
    [_promoView.leadingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [_promoView.trailingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [_promoView.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor
                       constant:-2 * kHorizontalMargin],
  ]];
}

@end
