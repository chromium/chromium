// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GeminiModalContentViewController {
  UIView* _contentView;
  UIBarButtonItem* _closeButton;
}

- (instancetype)initWithContentView:(UIView*)contentView {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _contentView = contentView;
  }
  return self;
}

#pragma mark - Public

- (void)setCloseButtonAccessibilityLabel:
    (NSString*)closeButtonAccessibilityLabel {
  _closeButtonAccessibilityLabel = [closeButtonAccessibilityLabel copy];
  _closeButton.accessibilityLabel = _closeButtonAccessibilityLabel;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Configure close button.
  _closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(closeButtonTapped)];
  _closeButton.accessibilityIdentifier =
      kGeminiModalContentCloseButtonAccessibilityIdentifier;
  _closeButton.accessibilityLabel = self.closeButtonAccessibilityLabel;
  self.navigationItem.rightBarButtonItem = _closeButton;

  // Configure content view.
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentView];
  AddSameConstraintsToSides(
      _contentView, self.view.safeAreaLayoutGuide,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(_contentView, self.view, LayoutSides::kBottom);
}

#pragma mark - Private

// Notifies the delegate that the user requested to close the modal.
- (void)closeButtonTapped {
  [self.delegate geminiModalContentViewControllerDidTapClose:self];
}

@end
