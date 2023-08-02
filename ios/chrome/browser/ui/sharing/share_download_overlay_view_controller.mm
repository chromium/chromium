// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/share_download_overlay_view_controller.h"

#import "ios/chrome/browser/shared/public/commands/share_download_overlay_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Alpha value for the background view.
const CGFloat kOverlayedViewBackgroundAlpha = 0.6;

// Width of the label displayed on the view as a percentage of the view's width.
const CGFloat kOverlayedViewLabelWidthPercentage = 0.7;

// Bottom margin for the label displayed on the view.
const CGFloat kOverlayedViewLabelBottomMargin = 60;

}  // namespace

@interface ShareDownloadOverlayViewController () <UIGestureRecognizerDelegate> {
}

// Handler that will manage user action.
@property(nonatomic, weak) id<ShareDownloadOverlayCommands> handler;

// Base view on which the overlay will be presented.
@property(nonatomic, weak) UIView* baseView;

@end

@implementation ShareDownloadOverlayViewController

- (instancetype)initWithBaseView:(UIView*)baseView
                         handler:(id<ShareDownloadOverlayCommands>)handler {
  self = [super init];
  if (self) {
    _baseView = baseView;
    _handler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];

  [self.view addSubview:[self createBackgroundView]];
  UILabel* label = [self createLabel];
  [self.view addSubview:label];
  [self.view addSubview:[self createSpinner]];
  [self.view addGestureRecognizer:[self createGestureRecognizer]];

  CGFloat labelWidth =
      [self.view frame].size.width * kOverlayedViewLabelWidthPercentage;
  [NSLayoutConstraint activateConstraints:@[
    [label.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:-kOverlayedViewLabelBottomMargin],
    [label.widthAnchor constraintEqualToConstant:labelWidth],
    [label.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
  ]];

  [self.baseView addSubview:self.view];
}

#pragma mark - Gesture events

// Called by the Done button from the navigation bar.
- (void)cancel {
  [self.handler cancelDownload];
}

#pragma mark - UIGestureRecognizerDelegate Methods

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  return YES;
}

#pragma mark - Private

// Creates a background view.
- (UIView*)createBackgroundView {
  UIView* grayBackgroundView = [[UIView alloc] initWithFrame:[self.view frame]];
  [grayBackgroundView setBackgroundColor:[UIColor darkGrayColor]];
  [grayBackgroundView setAlpha:kOverlayedViewBackgroundAlpha];
  [grayBackgroundView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                           UIViewAutoresizingFlexibleHeight)];
  return grayBackgroundView;
}

// Creates a spinner.
- (UIActivityIndicatorView*)createSpinner {
  UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
  [spinner setFrame:[self.view frame]];
  [spinner setHidesWhenStopped:YES];
  [spinner setUserInteractionEnabled:NO];
  [spinner startAnimating];
  [spinner setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight)];
  return spinner;
}

// Creates a label that indicate to the user how to cancel the download.
- (UILabel*)createLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  [label setTextColor:[UIColor whiteColor]];
  [label setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
  [label setNumberOfLines:0];
  [label setShadowColor:[UIColor blackColor]];
  [label setShadowOffset:CGSizeMake(0.0, 1.0)];
  [label setBackgroundColor:[UIColor clearColor]];
  [label setText:l10n_util::GetNSString(IDS_IOS_OPEN_IN_FILE_DOWNLOAD_CANCEL)];
  [label setLineBreakMode:NSLineBreakByWordWrapping];
  [label setTextAlignment:NSTextAlignmentCenter];
  return label;
}

// Creates a gesture recognizer that will call the cancel method if the user
// touch the screen.
- (UITapGestureRecognizer*)createGestureRecognizer {
  UITapGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(cancel)];
  [tapRecognizer setDelegate:self];
  return tapRecognizer;
}

@end
