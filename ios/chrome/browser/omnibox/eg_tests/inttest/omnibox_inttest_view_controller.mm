// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_view_controller_delegate.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Height of the omnibox container.
const CGFloat kOmniboxContainerHeight = 50;

}  // namespace

@interface OmniboxInttestViewController ()

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

@end

@implementation OmniboxInttestViewController {
  /// Container for the omnibox.
  UIView* _omniboxContainer;
  /// Cancel button.
  UIButton* _cancelButton;
  /// StackView for the`_omniboxContainer` and `_cancelButton`.
  UIStackView* _horizontalStackView;
  /// Container for the omnibox popup.
  UIButton* _omniboxPopupContainer;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _omniboxPopupContainer = [[UIButton alloc] init];
    // Initialize `setEditView` dependencies as it can be called before
    // `viewDidLoad`.
    _omniboxContainer = [[UIView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Omnibox popup container.
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxPopupContainer.layer.zPosition = 1;
  _omniboxPopupContainer.clipsToBounds = YES;
  [self.view addSubview:_omniboxPopupContainer];

  // Omnibox container.
  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  // Cancel button.
  _cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  _cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  [_cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                 forState:UIControlStateNormal];
  [_cancelButton addTarget:self
                    action:@selector(didTapCancelButton:)
          forControlEvents:UIControlEventTouchUpInside];

  // Horizontal stack view.
  _horizontalStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _omniboxContainer, _cancelButton ]];
  _horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _horizontalStackView.distribution = UIStackViewDistributionFill;
  [self.view addSubview:_horizontalStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_horizontalStackView.heightAnchor
        constraintEqualToConstant:kOmniboxContainerHeight],
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:_horizontalStackView.bottomAnchor],
  ]];
  using enum LayoutSides;
  AddSameConstraintsToSides(_horizontalStackView, self.view.safeAreaLayoutGuide,
                            kLeading | kTop | kTrailing);
  AddSameConstraintsToSides(_omniboxPopupContainer, self.view,
                            kLeading | kBottom | kTrailing);
}

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  _editView = editView;
  _editView.translatesAutoresizingMaskIntoConstraints = NO;
  [_omniboxContainer addSubview:_editView];
  AddSameConstraints(_editView, _omniboxContainer);
}

#pragma mark - OmniboxPopupPresenterDelegate

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return _omniboxPopupContainer;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
}

#pragma mark - Private

/// Handles cancel button taps.
- (void)didTapCancelButton:(UIView*)button {
  [self.delegate viewControllerDidTapCancelButton:self];
}

@end
