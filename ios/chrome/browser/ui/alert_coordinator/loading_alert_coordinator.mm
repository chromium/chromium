// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/loading_alert_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "base/mac/scoped_block.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// MDC constraints.
const CGFloat kMDCPadding = 24.0;
const CGFloat kMDCMargin = 40.0;
const CGFloat kButtonPadding = 8.0;

// Size of the activity indicator.
const CGFloat kActivityIndicatorPadding = 50.0;

// Constant for the title
const int kTitleLabelFontColor = 0x333333;
const int kTitleLabelFontSize = 16;

// Maximum size of the dialog
const CGFloat kPrefWidth = 330;
const CGFloat kPrefHeight = 300;

}  // namespace

@interface LoadingAlertCoordinator () {
  // Title of the alert.
  NSString* _title;
  // Callback for the cancel button.
  ProceduralBlock _cancelHandler;
  // View Controller which will be displayed on |baseViewController|.
  UIViewController* _presentedViewController;
}

// Callback called when the cancel button is pressed.
- (void)cancelCallback;

@end

// View Controller handling the layout of the dialog.
@interface LoadingViewController : UIViewController {
  // Title of the dialog.
  NSString* _title;
  // View containing the elements of the dialog.
  UIView* _contentView;
  // Coordinator used for the cancel callback.
  __weak LoadingAlertCoordinator* _coordinator;
  // Transitioning delegate for this ViewController.
  MDCDialogTransitionController* _transitionDelegate;
}

// Initializes with the |title| of the dialog and the |coordinator| which will
// be notified if the cancel callback occurs.
- (instancetype)initWithTitle:(NSString*)title
                  coordinator:(LoadingAlertCoordinator*)coordinator;

// Returns the maximum possible width for the dialog, taking into account the
// MDC constraints.
- (CGFloat)maxDialogWidth;
// Callback for the cancel button, calling the coordinator's callback.
- (void)cancelCallback;

@end

@implementation LoadingViewController

- (instancetype)initWithTitle:(NSString*)title
                  coordinator:(LoadingAlertCoordinator*)coordinator {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _title = [title copy];
    _coordinator = coordinator;
    self.modalPresentationStyle = UIModalPresentationCustom;
    _transitionDelegate = [[MDCDialogTransitionController alloc] init];
    self.transitioningDelegate = _transitionDelegate;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.preferredContentSize = CGSizeMake([self maxDialogWidth], kPrefHeight);
  self.view.backgroundColor = [UIColor whiteColor];

  // Cancel button.
  NSString* cancelTitle = l10n_util::GetNSString(IDS_CANCEL);
  MDCFlatButton* cancelButton = [[MDCFlatButton alloc] init];
  [cancelButton sizeToFit];
  [cancelButton setTitleColor:[UIColor blackColor]
                     forState:UIControlStateNormal];
  [cancelButton setTitle:cancelTitle forState:UIControlStateNormal];
  [cancelButton addTarget:self
                   action:@selector(cancelCallback)
         forControlEvents:UIControlEventTouchUpInside];

  // Activity indicator.
  MDCActivityIndicator* activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  [activityIndicator setCycleColors:ActivityIndicatorBrandedCycleColors()];
  [activityIndicator startAnimating];

  // Title.
  NSMutableDictionary* attrsDictionary = [NSMutableDictionary
      dictionaryWithObject:[[MDCTypography fontLoader]
                               mediumFontOfSize:kTitleLabelFontSize]
                    forKey:NSFontAttributeName];
  [attrsDictionary setObject:UIColorFromRGB(kTitleLabelFontColor)
                      forKey:NSForegroundColorAttributeName];

  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:_title
                                             attributes:attrsDictionary];

  UILabel* title = [[UILabel alloc] initWithFrame:CGRectZero];
  title.attributedText = string;

  // Content view.
  _contentView = [[UIView alloc] initWithFrame:CGRectZero];

  // Constraints.
  [activityIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
  [cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [title setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_contentView setTranslatesAutoresizingMaskIntoConstraints:NO];

  [_contentView addSubview:title];
  [_contentView addSubview:activityIndicator];
  [_contentView addSubview:cancelButton];

  [[self view] addSubview:_contentView];

  NSDictionary* viewsDictionary = @{
    @"spinner" : activityIndicator,
    @"cancel" : cancelButton,
    @"title" : title,
    @"contentView" : _contentView
  };
  NSDictionary* metrics = @{
    @"padding" : @(kMDCPadding),
    @"buttonPadding" : @(kButtonPadding),
    @"spinnerButtonPadding" : @(kButtonPadding + kMDCPadding),
    @"spinnerPadding" : @(kActivityIndicatorPadding)
  };

  NSString* verticalConstaint = @"V:|-(padding)-[title]-(spinnerPadding)-["
                                @"spinner]-(spinnerButtonPadding)-[cancel]-("
                                @"buttonPadding)-|";

  NSArray* constraints = @[
    verticalConstaint, @"H:|-(>=padding)-[title]-(>=padding)-|",
    @"H:|-[spinner]-|", @"H:[cancel]-(buttonPadding)-|", @"V:|[contentView]",
    @"H:|[contentView]|"
  ];
  ApplyVisualConstraintsWithMetrics(constraints, viewsDictionary, metrics);

  [_contentView addConstraint:[NSLayoutConstraint
                                  constraintWithItem:title
                                           attribute:NSLayoutAttributeCenterX
                                           relatedBy:NSLayoutRelationEqual
                                              toItem:_contentView
                                           attribute:NSLayoutAttributeCenterX
                                          multiplier:1.f
                                            constant:0.f]];
}

- (CGFloat)maxDialogWidth {
  CGFloat screenWidth = [[UIScreen mainScreen] bounds].size.width;
  return MIN(kPrefWidth, screenWidth - 2 * kMDCMargin);
}

- (void)viewDidLayoutSubviews {
  CGSize currentSize =
      CGSizeMake([self maxDialogWidth], [_contentView frame].size.height);
  self.preferredContentSize = currentSize;
}

- (void)cancelCallback {
  [_coordinator cancelCallback];
}
@end

@implementation LoadingAlertCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                             cancelHandler:(ProceduralBlock)cancelHandler {
  self = [super initWithBaseViewController:viewController browserState:nullptr];
  if (self) {
    _title = [title copy];
    _cancelHandler = cancelHandler;
  }
  return self;
}

- (void)start {
  _presentedViewController =
      [[LoadingViewController alloc] initWithTitle:_title coordinator:self];
  [self.baseViewController presentViewController:_presentedViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [[_presentedViewController presentingViewController]
      dismissViewControllerAnimated:NO
                         completion:nil];
  _presentedViewController = nil;
  _cancelHandler = nil;
}

- (void)cancelCallback {
  if (_cancelHandler)
    _cancelHandler();
  [self stop];
}

@end
