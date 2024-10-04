// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_progress_bar.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_mutator.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/text_field_view_containing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Top padding for the view content.
const CGFloat kViewTopPadding = 22;

/// Width of the back button.
const CGFloat kBackButtonWidth = 44;
/// Size of the back button.
const CGFloat kBackButtonSize = 24;

/// Horizontal inset of the cancel button.
const CGFloat kCancelButtonHorizontalInset = 8;
/// Font size for the cancel button.
const CGFloat kCancelButtonFontSize = 15;

/// Minimum leading and trailing padding for the omnibox container.
const CGFloat kOmniboxContainerHorizontalPadding = 10;

/// Minimum height of the omnibox container.
const CGFloat kOmniboxContainerMinimumHeight = 52;
/// Corner radius of the omnibox container.
const CGFloat kOmniboxContainerCornerRadius = 26;
/// Padding between the omnibox and the web container.
const CGFloat kWebContainerTopPadding = 16;

/// Height of the progress bar.
const CGFloat kProgressBarHeight = 2.0f;
/// Value of a full progress bar.
const CGFloat kProgressBarFull = 1.0f;

/// The duration for buttons appear & disappear animations.
const CGFloat kButtonAnimationDuration = 0.2f;

}  // namespace

@interface LensResultPageViewController ()

/// Web view in `_webViewContainer`.
@property(nonatomic, strong) UIView* webView;

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

/// Whether the back button is available. The back button might be available but
/// hidden when the omnibox is focused.
@property(nonatomic, assign) BOOL canGoBack;

/// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL omniboxFocused;

@end

@implementation LensResultPageViewController {
  /// Back button.
  UIButton* _backButton;
  /// Container for the omnibox.
  UIView* _omniboxContainer;
  /// Cancel button.
  UIButton* _cancelButton;
  /// StackView for the `_backButton`, `_omniboxContainer` and `_cancelButton`.
  UIStackView* _horizontalStackView;
  /// Container for the omnibox popup.
  UIButton* _omniboxPopupContainer;
  /// Button to focus the omnibox.
  UIButton* _omniboxTapTarget;
  /// Loading progress bar.
  LensOverlayProgressBar* _progressBar;
  /// Whether the web view should be hidden.
  BOOL _webViewHidden;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webViewContainer = [[UIView alloc] init];
    _omniboxPopupContainer = [[UIButton alloc] init];

    // Initialize `setEditView` dependencies as it can be called before
    // `viewDidLoad`.
    _omniboxContainer = [[UIView alloc] init];
    _omniboxTapTarget = [[UIButton alloc] init];
    _progressBar = [[LensOverlayProgressBar alloc] init];
    [_omniboxContainer addSubview:_omniboxTapTarget];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  CHECK(self.webViewContainer, kLensOverlayNotFatalUntil);
  // Webview container.
  self.webViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.webViewContainer.clipsToBounds = YES;
  [self.view addSubview:self.webViewContainer];

  // Omnibox popup container.
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.layer.zPosition = 1;
  _omniboxPopupContainer.clipsToBounds = YES;
  [_omniboxPopupContainer addTarget:self
                             action:@selector(didTapOmniboxPopupContainer:)
                   forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_omniboxPopupContainer];

  // Back Button.
  UIImage* image =
      DefaultSymbolWithPointSize(kChevronBackwardSymbol, kBackButtonSize);
  UIButtonConfiguration* backButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  backButtonConfiguration.image = image;
  // Constant to visually center the image as it's slightly left aligned.
  backButtonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(0, 4, 0, 0);
  __weak id<LensToolbarMutator> weakToolbarMutator = self.toolbarMutator;
  _backButton = [UIButton
      buttonWithConfiguration:backButtonConfiguration
                primaryAction:[UIAction actionWithHandler:^(UIAction* action) {
                  [weakToolbarMutator goBack];
                }]];
  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  _backButton.hidden = YES;

  // Omnibox container.
  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  _omniboxContainer.layer.cornerRadius = kOmniboxContainerCornerRadius;
  _omniboxContainer.clipsToBounds = YES;
  [_omniboxContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];

  // Omnibox tap target.
  _omniboxTapTarget.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxTapTarget.backgroundColor = UIColor.clearColor;
  [_omniboxTapTarget addTarget:self
                        action:@selector(didTapOmniboxTapTarget:)
              forControlEvents:UIControlEventTouchUpInside];
  AddSameConstraints(_omniboxContainer, _omniboxTapTarget);

  // Cancel button.
  _cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  _cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  [_cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_cancelButton setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
  UIFont* font = [UIFont systemFontOfSize:kCancelButtonFontSize];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_CANCEL)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;
  _cancelButton.configuration = buttonConfiguration;
  [self setCancelButtonHidden:YES animated:NO];
  [_cancelButton addTarget:self
                    action:@selector(didTapCancelButton:)
          forControlEvents:UIControlEventTouchUpInside];

  // Horizontal stack view.
  _horizontalStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _backButton, _omniboxContainer, _cancelButton
  ]];
  _horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _horizontalStackView.distribution = UIStackViewDistributionFill;
  [self.view addSubview:_horizontalStackView];

  // Progress bar.
  _progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_progressBar setHidden:YES animated:NO completion:nil];
  [_omniboxContainer addSubview:_progressBar];

  NSLayoutConstraint* omniboxLeadingConstraint =
      [_omniboxContainer.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor
                         constant:kOmniboxContainerHorizontalPadding];
  omniboxLeadingConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [_horizontalStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kViewTopPadding],
    [_backButton.widthAnchor constraintEqualToConstant:kBackButtonWidth],
    [_horizontalStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kOmniboxContainerMinimumHeight],
    [_backButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    omniboxLeadingConstraint,
    [self.view.trailingAnchor
        constraintEqualToAnchor:_horizontalStackView.trailingAnchor
                       constant:kOmniboxContainerHorizontalPadding],
    [_webViewContainer.topAnchor
        constraintEqualToAnchor:_horizontalStackView.bottomAnchor
                       constant:kWebContainerTopPadding],
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:_horizontalStackView.bottomAnchor],
    [_progressBar.leadingAnchor
        constraintEqualToAnchor:_omniboxContainer.leadingAnchor],
    [_progressBar.trailingAnchor
        constraintEqualToAnchor:_omniboxContainer.trailingAnchor],
    [_progressBar.bottomAnchor
        constraintEqualToAnchor:_omniboxContainer.bottomAnchor],
    [_progressBar.heightAnchor constraintEqualToConstant:kProgressBarHeight],
  ]];
  AddSameConstraintsToSides(
      self.webViewContainer, self.view,
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      _omniboxPopupContainer, self.view,
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.self ]
                       withAction:@selector(updateMutatorDarkMode)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self updateMutatorDarkMode];
  }
}
#endif

- (void)setWebViewHidden:(BOOL)hidden {
  if (_webViewHidden == hidden) {
    return;
  }

  _webViewHidden = hidden;
  _webView.hidden = hidden;
}

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  CHECK(!_editView, kLensOverlayNotFatalUntil);
  CHECK(editView, kLensOverlayNotFatalUntil);
  CHECK(_omniboxContainer, kLensOverlayNotFatalUntil);
  _editView = editView;
  _editView.translatesAutoresizingMaskIntoConstraints = NO;
  [_omniboxContainer insertSubview:_editView belowSubview:_omniboxTapTarget];
  AddSameConstraints(_editView, _omniboxContainer);
}

- (void)setMutator:(id<LensResultPageMutator>)mutator {
  _mutator = mutator;
  [self updateMutatorDarkMode];
}

- (void)setCancelButtonHidden:(BOOL)hidden animated:(BOOL)animated {
  if (_cancelButton.hidden == hidden) {
    return;
  }

  if (!animated) {
    _cancelButton.hidden = hidden;
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kButtonAnimationDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     __typeof(self) strongSelf = weakSelf;
                     if (!strongSelf) {
                       return;
                     }

                     strongSelf->_cancelButton.hidden = hidden;
                     [strongSelf->_horizontalStackView layoutIfNeeded];
                   }
                   completion:nil];
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  // Capture key command close when the omnibox is focused to defocus the
  // omnibox instead of closing the overlay.
  return !_omniboxPopupContainer.hidden;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self.toolbarMutator defocusOmnibox];
}

#pragma mark - LensResultPageConsumer

- (void)setWebView:(UIView*)webView {
  if (_webView == webView) {
    return;
  }

  if (_webView.superview == self.webViewContainer) {
    [_webView removeFromSuperview];
  }
  _webView = webView;
  _webView.hidden = _webViewHidden;

  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  if (!_webView || !self.webViewContainer) {
    return;
  }

  [self.webViewContainer addSubview:_webView];
  AddSameConstraints(_webView, self.webViewContainer);
}

- (void)setLoadingProgress:(float)progress {
  [self updateProgressBarVisibilityForProgress:progress];
  [_progressBar setProgress:progress animated:YES completion:nil];
}

- (void)updateProgressBarVisibilityForProgress:(float)progress {
  BOOL isLoading = progress != kProgressBarFull;
  BOOL shouldShowProgressBar = isLoading && _progressBar.hidden;
  BOOL shouldHideProgressBar = !isLoading && !_progressBar.hidden;

  if (shouldShowProgressBar) {
    [_progressBar setHidden:NO animated:YES completion:nil];
  } else if (shouldHideProgressBar) {
    [_progressBar setHidden:YES animated:YES completion:nil];
  }
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

#pragma mark - LensToolbarConsumer

- (void)setOmniboxFocused:(BOOL)isFocused {
  _omniboxFocused = isFocused;
  [self updateBackButtonVisibilityAnimated:YES];

  // Visible when omnibox is focused.
  [self setCancelButtonHidden:!isFocused animated:YES];

  _omniboxPopupContainer.hidden = !isFocused;

  // Hidden when omnibox is focused.
  _omniboxTapTarget.hidden = isFocused;
}

- (void)setCanGoBack:(BOOL)canGoBack {
  _canGoBack = canGoBack;
  [self updateBackButtonVisibilityAnimated:YES];
}

#pragma mark - Private

/// Handles omnibox tap target taps.
- (void)didTapOmniboxTapTarget:(UIView*)view {
  [self.toolbarMutator focusOmnibox];
}

/// Handles omnibox popup container taps, acting like a typing shield.
- (void)didTapOmniboxPopupContainer:(UIView*)view {
  [self.toolbarMutator defocusOmnibox];
}

/// Handles cancel button taps.
- (void)didTapCancelButton:(UIView*)button {
  [self.toolbarMutator defocusOmnibox];
}

- (void)updateBackButtonVisibilityAnimated:(BOOL)animated {
  BOOL hidden = self.omniboxFocused || !self.canGoBack;

  if (_backButton.hidden == hidden) {
    return;
  }

  if (!animated) {
    _backButton.hidden = hidden;
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kButtonAnimationDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     __typeof(self) strongSelf = weakSelf;
                     if (!strongSelf) {
                       return;
                     }

                     strongSelf->_backButton.hidden = hidden;
                     [strongSelf->_horizontalStackView layoutIfNeeded];
                   }
                   completion:nil];
}

/// Updates the user interface style in the mutator.
- (void)updateMutatorDarkMode {
  [self.mutator setIsDarkMode:self.traitCollection.userInterfaceStyle ==
                              UIUserInterfaceStyleDark];
}

@end
