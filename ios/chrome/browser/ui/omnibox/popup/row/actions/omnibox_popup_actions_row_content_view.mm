
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_view.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/attributed_string_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/actions_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ui/base/device_form_factor.h"

namespace {

const CGFloat kTextTopMargin = 6.0;
const CGFloat kMultilineTextTopMargin = 12.0;
/// Trailing margin of the text. This margin is increased when the text is on
/// multiple lines, otherwise text of the first lines without the gradient seems
/// too close to the trailing (button/end).
const CGFloat kTextTrailingMargin = 0.0;
const CGFloat kMultilineTextTrailingMargin = 4.0;
const CGFloat kMultilineLineSpacing = 2.0;
const CGFloat kTrailingButtonSize = 24;
const CGFloat kTrailingButtonTrailingMargin = 14;
/// Trailing button trailing margin with popout omnibox.
const CGFloat kTrailingButtonTrailingMarginPopout = 22.0;
const CGFloat kTextSpacing = 2.0f;
const CGFloat kLeadingIconViewSize = 30.0f;
const CGFloat kLeadingSpace = 17.0f;
/// Leading space with popout omnibox.
const CGFloat kLeadingSpacePopout = 23.0;
const CGFloat kTextIconSpace = 14.0f;
/// Top color opacity of the `_selectedBackgroundView`.
const CGFloat kTopGradientColorOpacity = 0.85;
/// The rich entity height
const CGFloat kRichEntityViewHeight = 52;
/// The minimum height of the row.
const CGFloat kActionsRowMinimumHeight = 98;
/// The space between the actions scroll view and the separator
const CGFloat kActionScrollViewSeparatorSpace = 8;

}  // namespace

@implementation OmniboxPopupActionsRowContentView {
  FadeTruncatingLabel* _primaryLabel;
  FadeTruncatingLabel* _secondaryLabelFading;
  UILabel* _secondaryLabelTruncating;
  OmniboxIconView* _leadingIconView;
  ExtendedTouchTargetButton* _trailingButton;
  UIStackView* _textStackView;
  UIView* _separator;
  UIView* _selectedBackgroundView;
  /// The Actions  view (contains the actions buttons).
  ActionsView* _actionsView;
  /// The Rich entity view (contains the leading icon,text and the trailing
  /// icon).
  UIView* _richEntityView;

  NSLayoutConstraint* _separatorHeightConstraint;
  /// Constraints that changes when the text is a multi-lines search suggestion.
  NSLayoutConstraint* _textTopConstraint;
  NSLayoutConstraint* _textTrailingToButtonConstraint;
  NSLayoutConstraint* _textTrailingConstraint;
  /// Constraints changes with popout omnibox.
  NSLayoutConstraint* _leadingConstraint;
  NSLayoutConstraint* _trailingButtonTrailingConstraint;
}

- (instancetype)initWithConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    // Background.
    self.backgroundColor = UIColor.clearColor;
    _selectedBackgroundView = [[GradientView alloc]
        initWithTopColor:
            [[UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"]
                colorWithAlphaComponent:kTopGradientColorOpacity]
             bottomColor:
                 [UIColor
                     colorNamed:@"omnibox_suggestion_row_highlight_color"]];
    _selectedBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _selectedBackgroundView.layer.zPosition = -1;
    _selectedBackgroundView.hidden = YES;
    [self addSubview:_selectedBackgroundView];
    AddSameConstraints(self, _selectedBackgroundView);
    // Rich entity view.
    _richEntityView = [[UIView alloc] init];
    _richEntityView.translatesAutoresizingMaskIntoConstraints = NO;
    // Leading Icon.
    _leadingIconView = [[OmniboxIconView alloc] init];
    _leadingIconView.imageRetriever = configuration.imageRetriever;
    _leadingIconView.faviconRetriever = configuration.faviconRetriever;
    _leadingIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [_richEntityView addSubview:_leadingIconView];
    // Primary Label.
    _primaryLabel = [[FadeTruncatingLabel alloc] init];
    _primaryLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_primaryLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:UILayoutConstraintAxisVertical];
    [_primaryLabel setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisVertical];
    _primaryLabel.lineSpacing = kMultilineLineSpacing;

    // Secondary Label Fading.
    _secondaryLabelFading = [[FadeTruncatingLabel alloc] init];
    _secondaryLabelFading.translatesAutoresizingMaskIntoConstraints = NO;
    _secondaryLabelFading.hidden = YES;
    [_secondaryLabelFading
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisVertical];

    // Secondary Label Truncating.
    _secondaryLabelTruncating = [[UILabel alloc] init];
    _secondaryLabelTruncating.translatesAutoresizingMaskIntoConstraints = NO;
    _secondaryLabelTruncating.lineBreakMode = NSLineBreakByTruncatingTail;
    _secondaryLabelTruncating.hidden = YES;
    [_secondaryLabelTruncating
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisVertical];

    // Text Stack View.
    _textStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _primaryLabel, _secondaryLabelFading, _secondaryLabelTruncating
    ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.alignment = UIStackViewAlignmentFill;
    _textStackView.spacing = kTextSpacing;
    [_richEntityView addSubview:_textStackView];

    // Trailing Button.
    _trailingButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingButton.isAccessibilityElement = NO;
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonTapped)
              forControlEvents:UIControlEventTouchUpInside];
    _trailingButton.hidden = YES;  // Optional view.
    [_richEntityView addSubview:_trailingButton];
    _actionsView = [[ActionsView alloc] initWithConfiguration:configuration];

    UIStackView* suggestionContentVerticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _richEntityView, _actionsView ]];
    suggestionContentVerticalStackView
        .translatesAutoresizingMaskIntoConstraints = NO;
    [suggestionContentVerticalStackView
        setDistribution:UIStackViewDistributionFillProportionally];
    suggestionContentVerticalStackView.axis = UILayoutConstraintAxisVertical;

    [self addSubview:suggestionContentVerticalStackView];

    // Bottom separator.
    _separator = [[UIView alloc] initWithFrame:CGRectZero];
    _separator.translatesAutoresizingMaskIntoConstraints = NO;
    _separator.hidden = YES;
    _separator.backgroundColor = [UIColor
        colorNamed:ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
                       ? kOmniboxPopoutSuggestionRowSeparatorColor
                       : kOmniboxSuggestionRowSeparatorColor];
    [self addSubview:_separator];

    // Top space should be at least the given top margin, but can be more if
    // the row is short enough to use the minimum height constraint above.
    _textTopConstraint = [_textStackView.topAnchor
        constraintEqualToAnchor:_richEntityView.topAnchor
                       constant:kTextTopMargin];
    _textTopConstraint.priority = UILayoutPriorityRequired - 1;

    // When there is no trailing button, the text should extend to the cell's
    // trailing edge with a padding.
    _textTrailingConstraint = [_richEntityView.trailingAnchor
        constraintEqualToAnchor:_textStackView.trailingAnchor
                       constant:kTextTrailingMargin];
    _textTrailingConstraint.priority = UILayoutPriorityRequired - 1;

    // The trailing button is optional. Constraint is activated when needed.
    _textTrailingToButtonConstraint = [_trailingButton.leadingAnchor
        constraintEqualToAnchor:_textStackView.trailingAnchor
                       constant:kTextTrailingMargin];

    // Constraint updated with popout omnibox.
    _trailingButtonTrailingConstraint = [_richEntityView.trailingAnchor
        constraintEqualToAnchor:_trailingButton.trailingAnchor
                       constant:kTrailingButtonTrailingMargin];
    _leadingConstraint = [_leadingIconView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kLeadingSpace];

    [NSLayoutConstraint activateConstraints:@[
      [_richEntityView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kRichEntityViewHeight],
      [self.heightAnchor
          constraintGreaterThanOrEqualToConstant:kActionsRowMinimumHeight],
      [_richEntityView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_richEntityView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_richEntityView.topAnchor constraintEqualToAnchor:self.topAnchor],

      // Position leadingIconView at the leading edge of the view.
      [_leadingIconView.widthAnchor
          constraintEqualToConstant:kLeadingIconViewSize],
      [_leadingIconView.heightAnchor
          constraintEqualToConstant:kLeadingIconViewSize],
      _leadingConstraint,

      // Position textStackView "after" leadingIconView.
      _textTopConstraint, _textTrailingConstraint,
      [_textStackView.leadingAnchor
          constraintEqualToAnchor:_leadingIconView.trailingAnchor
                         constant:kTextIconSpace],

      // Trailing button constraints.
      [_trailingButton.heightAnchor
          constraintEqualToConstant:kTrailingButtonSize],
      [_trailingButton.widthAnchor
          constraintEqualToConstant:kTrailingButtonSize],
      _trailingButtonTrailingConstraint,

      // Separator height anchor added in `didMoveToWindow`.
      [_separator.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [_separator.leadingAnchor
          constraintEqualToAnchor:_textStackView.leadingAnchor],
      [suggestionContentVerticalStackView.bottomAnchor
          constraintEqualToAnchor:_separator.topAnchor
                         constant:-kActionScrollViewSeparatorSpace],
      [_leadingIconView.centerYAnchor
          constraintEqualToAnchor:_richEntityView.centerYAnchor],
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:_richEntityView.centerYAnchor],
      [_trailingButton.centerYAnchor
          constraintEqualToAnchor:_richEntityView.centerYAnchor]
    ]];

    [self addInteraction:[[ViewPointerInteraction alloc] init]];
    self.configuration = configuration;
  }
  return self;
}

- (void)didMoveToWindow {
  if (self.window) {
    if (!_separatorHeightConstraint) {
      _separatorHeightConstraint = [_separator.heightAnchor
          constraintEqualToConstant:1.0f / self.window.screen.scale];
      _separatorHeightConstraint.active = YES;
    }
  }
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  // Prevents the cell from resetting the semanticContentAttribute before
  // display. This fixes a bug where the semanticContentAttribute is reset when
  // scrolling.
  if (semanticContentAttribute != _configuration.semanticContentAttribute) {
    return;
  }

  [super setSemanticContentAttribute:semanticContentAttribute];

  _trailingButton.semanticContentAttribute = semanticContentAttribute;
  _richEntityView.semanticContentAttribute = semanticContentAttribute;
  _actionsView.semanticContentAttribute = semanticContentAttribute;

  // Forces texts to have the same alignment as the omnibox textfield text.
  BOOL isRTL = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                           semanticContentAttribute] ==
               UIUserInterfaceLayoutDirectionRightToLeft;
  NSTextAlignment forcedTextAlignment =
      isRTL ? NSTextAlignmentRight : NSTextAlignmentLeft;
  _primaryLabel.textAlignment = forcedTextAlignment;
  _secondaryLabelFading.textAlignment = forcedTextAlignment;
  _secondaryLabelTruncating.textAlignment = forcedTextAlignment;
}

- (NSString*)accessibilityLabel {
  return _primaryLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return _configuration.secondaryTextFading
             ? _secondaryLabelFading.attributedText.string
             : _secondaryLabelTruncating.attributedText.string;
}

#pragma mark - UIContentView

- (void)setConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  // This is technically possible as configuration overrides
  // id<UIContentConfiguration>.
  if (![configuration
          isMemberOfClass:OmniboxPopupActionsRowContentConfiguration.class]) {
    return;
  }
  _configuration = [configuration copy];
  [self setupWithConfiguration:_configuration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration
      isMemberOfClass:OmniboxPopupActionsRowContentConfiguration.class];
}

#pragma mark - Private

- (void)setupWithConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  CHECK([configuration
      isMemberOfClass:OmniboxPopupActionsRowContentConfiguration.class]);

  [_actionsView updateConfiguration:configuration];

  // Background.
  _selectedBackgroundView.hidden = !configuration.isBackgroundHighlighted;

  // Leading Icon.
  [_leadingIconView prepareForReuse];
  [_leadingIconView setOmniboxIcon:configuration.leadingIcon];
  _leadingIconView.highlighted = configuration.leadingIconHighlighted;

  // Primary Label.
  _primaryLabel.attributedText = configuration.primaryText;
  _primaryLabel.numberOfLines = configuration.primaryTextNumberOfLines;

  // Secondary Label.
  _secondaryLabelFading.hidden = YES;
  _secondaryLabelTruncating.hidden = YES;
  if (configuration.secondaryText) {
    UILabel* secondaryLabel = configuration.secondaryTextFading
                                  ? _secondaryLabelFading
                                  : _secondaryLabelTruncating;
    secondaryLabel.hidden = NO;
    secondaryLabel.attributedText = configuration.secondaryText;
    secondaryLabel.numberOfLines = configuration.secondaryTextNumberOfLines;
    if (configuration.secondaryTextFading) {
      _secondaryLabelFading.displayAsURL =
          configuration.secondaryTextDisplayAsURL;
    }
  }

  // Trailing Button.
  if (configuration.trailingIcon) {
    [_trailingButton setImage:configuration.trailingIcon
                     forState:UIControlStateNormal];
    _trailingButton.hidden = NO;
    _trailingButton.tintColor = configuration.trailingIconTintColor;
    _trailingButton.accessibilityIdentifier =
        configuration.trailingButtonAccessibilityIdentifier;
    _textTrailingToButtonConstraint.active = YES;
  } else {
    _textTrailingToButtonConstraint.active = NO;
    _trailingButton.hidden = YES;
    _trailingButton.accessibilityIdentifier = nil;
  }

  // Separator.
  _separator.hidden = !configuration.showSeparator;

  // Text margins.
  if (configuration.primaryTextNumberOfLines > 1) {
    _textTrailingConstraint.constant = kMultilineTextTrailingMargin;
    _textTrailingToButtonConstraint.constant = kMultilineTextTrailingMargin;
    _textTopConstraint.constant = kMultilineTextTopMargin;
  } else {
    _textTrailingConstraint.constant = kTextTrailingMargin;
    _textTrailingToButtonConstraint.constant = kTextTrailingMargin;
    _textTopConstraint.constant = kTextTopMargin;
  }

  // Popout omnibox margins.
  if (configuration.isPopoutOmnibox) {
    _trailingButtonTrailingConstraint.constant =
        kTrailingButtonTrailingMarginPopout;
    _leadingConstraint.constant = kLeadingSpacePopout;
  } else {
    _trailingButtonTrailingConstraint.constant = kTrailingButtonTrailingMargin;
    _leadingConstraint.constant = kLeadingSpace;
  }

  self.directionalLayoutMargins = configuration.directionalLayoutMargin;
  self.semanticContentAttribute = configuration.semanticContentAttribute;
  [configuration.delegate
              omniboxPopupRowWithConfiguration:configuration
      didUpdateAccessibilityActionsAtIndexPath:configuration.indexPath];
}

/// Handles tap on trailing button.
- (void)trailingButtonTapped {
  [self.configuration.delegate
      omniboxPopupRowWithConfiguration:self.configuration
       didTapTrailingButtonAtIndexPath:self.configuration.indexPath];
}

@end
