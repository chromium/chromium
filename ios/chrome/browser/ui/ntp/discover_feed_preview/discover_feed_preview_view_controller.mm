// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_view_controller.h"

#import <MaterialComponents/MaterialProgressView.h>

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kURLBarMarginVertical = 13.0;
const CGFloat kURLBarMarginHorizontal = 16.0;
const CGFloat kSeparatorHeight = 0.1f;
const CGFloat kProgressBarHeight = 2.0f;
}  // namespace

@interface DiscoverFeedPreviewViewController ()

// The view of the loaded webState.
@property(nonatomic, strong) UIView* webStateView;

// The preview url.
@property(nonatomic, copy) NSString* URL;

// Progress bar displayed below the URL bar.
@property(nonatomic, strong) MDCProgressView* progressBar;

// YES if the page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;

@end

@implementation DiscoverFeedPreviewViewController

- (instancetype)initWithView:(UIView*)webStateView URL:(NSString*)URL {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _webStateView = webStateView;
    _URL = URL;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UILabel* URLBarLabel = [[UILabel alloc] init];
  URLBarLabel.text = self.URL;
  URLBarLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  URLBarLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  URLBarLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* URLBarView = [[UIView alloc] init];
  URLBarView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  URLBarView.translatesAutoresizingMaskIntoConstraints = NO;

  [URLBarView addSubview:URLBarLabel];
  [NSLayoutConstraint activateConstraints:@[
    [URLBarLabel.topAnchor constraintEqualToAnchor:URLBarView.topAnchor
                                          constant:kURLBarMarginVertical],
    [URLBarLabel.bottomAnchor constraintEqualToAnchor:URLBarView.bottomAnchor
                                             constant:-kURLBarMarginVertical],
    [URLBarLabel.leadingAnchor constraintEqualToAnchor:URLBarView.leadingAnchor
                                              constant:kURLBarMarginHorizontal],
    [URLBarLabel.trailingAnchor
        constraintEqualToAnchor:URLBarView.trailingAnchor
                       constant:-kURLBarMarginHorizontal],

  ]];

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;

  self.progressBar = [[MDCProgressView alloc] init];
  self.progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  self.progressBar.hidden = YES;

  self.webStateView.translatesAutoresizingMaskIntoConstraints = NO;

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
    [separator.bottomAnchor constraintEqualToAnchor:self.progressBar.topAnchor],
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

#pragma mark - DiscoverFeedPreviewConsumer

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
