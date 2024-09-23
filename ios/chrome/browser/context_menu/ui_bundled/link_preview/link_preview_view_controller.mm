// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_view_controller.h"

#import <MaterialComponents/MaterialProgressView.h>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {
const CGFloat kURLBarMarginVertical = 13.0;
const CGFloat kURLBarMarginHorizontal = 16.0;
const CGFloat kSeparatorHeight = 0.1f;
const CGFloat kProgressBarHeight = 2.0f;
}  // namespace

@interface LinkPreviewViewController ()

// The view of the loaded webState.
@property(nonatomic, strong) UIView* webStateView;

// The NSString that indicates the origin of the preview url.
@property(nonatomic, copy) NSString* origin;

// The URL bar label.
@property(nonatomic, strong) UILabel* URLBarLabel;

// Progress bar displayed below the URL bar.
@property(nonatomic, strong) MDCProgressView* progressBar;

// YES if the page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;

@end

@implementation LinkPreviewViewController

- (instancetype)initWithView:(UIView*)webStateView origin:(NSString*)origin {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _webStateView = webStateView;
    _origin = origin;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.URLBarLabel = [[UILabel alloc] init];
  self.URLBarLabel.text = self.origin;
  self.URLBarLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.URLBarLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.URLBarLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.URLBarLabel.accessibilityIdentifier = kPreviewOriginIdentifier;

  UIView* URLBarView = [[UIView alloc] init];
  URLBarView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  URLBarView.translatesAutoresizingMaskIntoConstraints = NO;
  URLBarView.accessibilityIdentifier = kPreviewURLBarIdentifier;

  [URLBarView addSubview:self.URLBarLabel];
  [NSLayoutConstraint activateConstraints:@[
    [self.URLBarLabel.topAnchor constraintEqualToAnchor:URLBarView.topAnchor
                                               constant:kURLBarMarginVertical],
    [self.URLBarLabel.bottomAnchor
        constraintEqualToAnchor:URLBarView.bottomAnchor
                       constant:-kURLBarMarginVertical],
    [self.URLBarLabel.leadingAnchor
        constraintEqualToAnchor:URLBarView.leadingAnchor
                       constant:kURLBarMarginHorizontal],
    [self.URLBarLabel.trailingAnchor
        constraintEqualToAnchor:URLBarView.trailingAnchor
                       constant:-kURLBarMarginHorizontal],

  ]];

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;

  self.progressBar = [[MDCProgressView alloc] init];
  self.progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  self.progressBar.hidden = YES;
  self.progressBar.accessibilityIdentifier = kPreviewProgressBarIdentifier;

  self.webStateView.translatesAutoresizingMaskIntoConstraints = NO;
  self.webStateView.accessibilityIdentifier = kPreviewWebStateViewIdentifier;

  [self.view addSubview:URLBarView];
  [self.view addSubview:separator];
  [self.view addSubview:self.progressBar];
  [self.view addSubview:self.webStateView];

  [NSLayoutConstraint activateConstraints:@[
    [URLBarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [URLBarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [URLBarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [URLBarView.bottomAnchor constraintEqualToAnchor:separator.topAnchor],
    [separator.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [separator.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(kSeparatorHeight)],
    [separator.bottomAnchor
        constraintEqualToAnchor:self.webStateView.topAnchor],
    [self.progressBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.progressBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.progressBar.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kProgressBarHeight)],
    [self.progressBar.bottomAnchor
        constraintEqualToAnchor:self.webStateView.topAnchor],
    [self.webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.webStateView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

#pragma mark - LinkPreviewConsumer

- (void)setLoadingState:(BOOL)loading {
  if (self.loading == loading)
    return;

  self.loading = loading;

  if (!loading) {
    [self finishProgressBar];
  } else if (self.progressBar.hidden) {
    [self.progressBar setProgress:0];
    [self updateProgressBarVisibility];
  }
}

- (void)setLoadingProgressFraction:(double)progress {
  [self.progressBar setProgress:progress animated:YES completion:nil];
}

- (void)setPreviewOrigin:(NSString*)origin {
  self.origin = origin;
  self.URLBarLabel.text = origin;
}

#pragma mark - Private

// Reset auto layout of the webStateView to YES, otherwise it can't be
// expanded to a tab.
- (void)resetAutoLayoutForPreview {
  self.webStateView.translatesAutoresizingMaskIntoConstraints = YES;
}

// Finish the progress bar when the page stops loading.
- (void)finishProgressBar {
  __weak __typeof(self) weakSelf = self;
  [self.progressBar setProgress:1
                       animated:YES
                     completion:^(BOOL finished) {
                       [weakSelf updateProgressBarVisibility];
                     }];
}

// Makes sure that the visibility of the progress bar is matching the one which
// is expected.
- (void)updateProgressBarVisibility {
  __weak __typeof(self) weakSelf = self;
  if (self.loading && self.progressBar.hidden) {
    [self.progressBar setHidden:NO
                       animated:YES
                     completion:^(BOOL finished) {
                       [weakSelf updateProgressBarVisibility];
                     }];
  } else if (!self.loading && !self.progressBar.hidden) {
    [self.progressBar setHidden:YES
                       animated:YES
                     completion:^(BOOL finished) {
                       [weakSelf updateProgressBarVisibility];
                     }];
  }
}

@end
