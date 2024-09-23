// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/snippet_search_engine_button.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Radius for the favicon container.
constexpr CGFloat kFaviconContainerViewRadius = 7.;
// Size for the favicon container.
constexpr CGFloat kFaviconContainerViewSize = 32.;
// Margin between the name and snippet labels.
constexpr CGFloat kNameSnippetLabelMargin = 2.;
// The size of the radio button size.
constexpr CGFloat kRadioButtonSize = 24.;
// The size of the radio button image.
constexpr CGFloat kRadioButtonImageSize = 20.;
// Upper vertical margin for name label in the button.
constexpr CGFloat kUpperVerticalMargin = 10.;
// Lower vertical margin for the snippet label in the button.
constexpr CGFloat kLowerVerticalMargin = 12.;
// Horizontal margin between elements in the button.
constexpr CGFloat kInnerHorizontalMargin = 12.;
// Horizontal margin between favicon/radio button image and the border.
constexpr CGFloat kBorderHorizontalMargin = 16.;
// Chevron button size.
constexpr CGFloat kChevronButtonSize = 44.;
// Horizontal chevron margin with the separtor, the name label and the snippet
// label.
constexpr CGFloat kChevronButtonHorizontalMargin = 2.;
// Thickness of the vertical separator.
constexpr CGFloat kSeparatorThickness = 1.;
// Duration of the snippet animation when changing state.
constexpr NSTimeInterval kSnippetAnimationDurationInSecond = .3;
// Alpha value for the checked background color.
constexpr NSTimeInterval kCheckedBackgroundColorAlpha = .1;

// Returns a snippet label.
UILabel* SnippetLabel() {
  UILabel* snippetLabel = [[UILabel alloc] init];
  snippetLabel.translatesAutoresizingMaskIntoConstraints = NO;
  snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  snippetLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  snippetLabel.adjustsFontForContentSizeCategory = YES;
  snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return snippetLabel;
}

// Background color for selected element.
UIColor* GetCheckedBackgroundColor() {
  return [[UIColor colorNamed:kBlueColor]
      colorWithAlphaComponent:kCheckedBackgroundColorAlpha];
}

// Color for the tint of the radio button of the selected element.
UIColor* GetCheckedTintColor() {
  return [UIColor colorNamed:kBlueColor];
}

}  // namespace

@implementation SnippetSearchEngineButton {
  // Container View for the faviconView.
  UIView* _faviconContainerView;
  UIImageView* _faviconImageView;
  UILabel* _nameLabel;
  SnippetButtonState _snippetButtonState;
  UIButton* _chevronButton;
  BOOL _isChevronButtonEnabled;
  UIImageView* _radioButtonImageView;
  // Horizontal separator, shown only if `horizontalSeparatorHidden` is NO.
  UIView* _horizontalSeparator;
  // UILabel to display the first line of the snippet.
  UILabel* _snippetLabelOneLine;
  // UILabel to display the snippet with all lines.
  UILabel* _snippetLabelExpanded;
  // Constraint to activate when the button is collapsed. This contraint
  // locks the bottom of `_snippetLabelOneLine` with the bottom of
  // SnippetSearchEngineButton (with the right margin).
  // `_snippetLabelExpandedConstraint` needs to be disabled.
  NSLayoutConstraint* _snippetLabelOneLineConstraint;
  // Constraint to activate when the button is expanded. This contraint
  // locks the bottom of `_snippetLabelExpanded` with the bottom of
  // SnippetSearchEngineButton (with the right margin).
  // `_snippetLabelOneLineConstraint` needs to be disabled.
  NSLayoutConstraint* _snippetLabelExpandedConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
    // Add the favicon container view and the favicon image view.
    _faviconContainerView = [[UIView alloc] init];
    _faviconContainerView.userInteractionEnabled = NO;
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconContainerView.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    _faviconContainerView.layer.cornerRadius = kFaviconContainerViewRadius;
    _faviconContainerView.layer.masksToBounds = YES;
    [self addSubview:_faviconContainerView];
    _faviconImageView = [[UIImageView alloc] init];
    _faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconImageView.layer.cornerRadius = kFaviconImageViewRadius;
    _faviconImageView.clipsToBounds = YES;
    [_faviconContainerView addSubview:_faviconImageView];
    [_faviconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    // Add name label.
    _nameLabel = [[UILabel alloc] init];
    _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _nameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _nameLabel.adjustsFontForContentSizeCategory = YES;
    [self addSubview:_nameLabel];
    // Make sure iOS prefers to strech the margins and not the name label,
    // by increasing the hugging priority.
    [_nameLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                                  forAxis:UILayoutConstraintAxisVertical];
    // Add one line snippet.
    _snippetLabelOneLine = SnippetLabel();
    _snippetLabelOneLine.numberOfLines = 1;
    // Make sure the snippet is not streched.
    [_snippetLabelOneLine
        setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                          forAxis:UILayoutConstraintAxisVertical];
    [self addSubview:_snippetLabelOneLine];
    // Add expanded snippet.
    _snippetLabelExpanded = SnippetLabel();
    _snippetLabelExpanded.numberOfLines = 0;
    // Make sure the snippet is not streched.
    [_snippetLabelExpanded
        setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                          forAxis:UILayoutConstraintAxisVertical];
    [self addSubview:_snippetLabelExpanded];
    // Add Chevron.
    _chevronButton = [[UIButton alloc] init];
    _chevronButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_chevronButton setImage:DefaultSymbolTemplateWithPointSize(
                                 kChevronDownSymbol, kSymbolAccessoryPointSize)
                    forState:UIControlStateNormal];

    _chevronButton.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
    [_chevronButton addTarget:self
                       action:@selector(chevronToggleAction:)
             forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_chevronButton];
    // Add vertical separator.
    UIView* verticalSeparator = [[UIView alloc] init];
    verticalSeparator.userInteractionEnabled = NO;
    verticalSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    verticalSeparator.backgroundColor = [UIColor colorNamed:kGrey300Color];
    [self addSubview:verticalSeparator];
    // Add Horizontal separator.
    _horizontalSeparator = [[UIView alloc] init];
    _horizontalSeparator.userInteractionEnabled = NO;
    _horizontalSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    _horizontalSeparator.backgroundColor = [UIColor colorNamed:kGrey300Color];
    [self addSubview:_horizontalSeparator];
    // Add the checked circle holder.
    _radioButtonImageView = [[UIImageView alloc] init];
    _radioButtonImageView.userInteractionEnabled = NO;
    _radioButtonImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_radioButtonImageView];
    // This layout guide is to generate the middle between _nameLabel and
    // _snippetLabelOneLine. This vertical middle is used to vertically aligned
    // the favicon, the chevron the vertical separator and the radio button.
    UILayoutGuide* nameAndOneLineSnippetLayoutGuide =
        [[UILayoutGuide alloc] init];
    [self addLayoutGuide:nameAndOneLineSnippetLayoutGuide];
    // Constraint when the snippet is expanded.
    _snippetLabelExpandedConstraint = [_snippetLabelExpanded.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kLowerVerticalMargin];
    // Constraint when the snippet is only one line.
    _snippetLabelOneLineConstraint =
        [nameAndOneLineSnippetLayoutGuide.bottomAnchor
            constraintEqualToAnchor:self.bottomAnchor
                           constant:-kLowerVerticalMargin];
    NSArray* constraints = @[
      // Constraints for avicon and favicon container.
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kBorderHorizontalMargin],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .centerYAnchor],
      [_faviconContainerView.widthAnchor
          constraintEqualToConstant:kFaviconContainerViewSize],
      [_faviconContainerView.heightAnchor
          constraintEqualToConstant:kFaviconContainerViewSize],
      [_faviconImageView.centerXAnchor
          constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
      [_faviconImageView.centerYAnchor
          constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
      [_faviconImageView.widthAnchor
          constraintEqualToConstant:kFaviconImageViewSize],
      [_faviconImageView.heightAnchor
          constraintEqualToConstant:kFaviconImageViewSize],
      // Constraints for layout guide for _nameLabel and _snippetLabelOneLine.
      [nameAndOneLineSnippetLayoutGuide.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kUpperVerticalMargin],
      _snippetLabelOneLineConstraint,
      [nameAndOneLineSnippetLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kInnerHorizontalMargin],
      [nameAndOneLineSnippetLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_chevronButton.leadingAnchor
                         constant:-kChevronButtonHorizontalMargin],
      // Constraints for name label.
      [_nameLabel.topAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide.topAnchor],
      [_nameLabel.leadingAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .leadingAnchor],
      [_nameLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                                .trailingAnchor],
      [_nameLabel.bottomAnchor
          constraintEqualToAnchor:_snippetLabelOneLine.topAnchor
                         constant:-kNameSnippetLabelMargin],
      // Constraints for _snippetLabelOneLine.
      [_snippetLabelOneLine.bottomAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .bottomAnchor],
      [_snippetLabelOneLine.leadingAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .leadingAnchor],
      [_snippetLabelOneLine.trailingAnchor
          constraintLessThanOrEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                                .trailingAnchor],
      // Constraints for _snippetLabelExpanded.
      [_snippetLabelExpanded.topAnchor
          constraintEqualToAnchor:_snippetLabelOneLine.topAnchor],
      [_snippetLabelExpanded.leadingAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .leadingAnchor],
      [_snippetLabelExpanded.trailingAnchor
          constraintLessThanOrEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                                .trailingAnchor],
      // Constraints for chevron.
      [_chevronButton.heightAnchor
          constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.widthAnchor constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.centerYAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .centerYAnchor],
      [_chevronButton.trailingAnchor
          constraintEqualToAnchor:verticalSeparator.leadingAnchor
                         constant:-kChevronButtonHorizontalMargin],
      // Constraints for vertical separator.
      [verticalSeparator.heightAnchor
          constraintEqualToAnchor:_nameLabel.heightAnchor],
      [verticalSeparator.centerYAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .centerYAnchor],
      [verticalSeparator.widthAnchor
          constraintEqualToConstant:kSeparatorThickness],
      [verticalSeparator.trailingAnchor
          constraintEqualToAnchor:_radioButtonImageView.leadingAnchor
                         constant:-kInnerHorizontalMargin],
      // Constraints for radio button.
      [_radioButtonImageView.centerYAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .centerYAnchor],
      [_radioButtonImageView.widthAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.heightAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kBorderHorizontalMargin],
      // Constraints for horizontal separator.
      [_horizontalSeparator.leadingAnchor
          constraintEqualToAnchor:nameAndOneLineSnippetLayoutGuide
                                      .leadingAnchor],
      [_horizontalSeparator.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_horizontalSeparator.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_horizontalSeparator.heightAnchor
          constraintEqualToConstant:kSeparatorThickness],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
    [self updateCellWithSnippetSate:SnippetButtonState::kOneLine animate:NO];
    [self updateCircleImageView];
    self.userInteractionEnabled = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
  }
  return self;
}

- (void)updateAccessibilityTraits {
  self.accessibilityTraits |= UIAccessibilityTraitButton;
  if (_checked) {
    self.accessibilityTraits |= UIAccessibilityTraitSelected;
  } else {
    self.accessibilityTraits &= ~UIAccessibilityTraitSelected;
  }
}

#pragma mark - UIView

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  self.backgroundColor = GetCheckedBackgroundColor();
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  if (!_checked) {
    // This case can happen if the user taps on the selected search engine,
    // and cancels the tap by moving the finger away. The button background
    // color needs to be selected.
    self.backgroundColor = nil;
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];
  // To know if the chevron is needed, we need to compare
  // `_snippetLabelExpanded` height and `_snippetLabelOneLine` height. To avoid
  // any issues with float approximations, there is one pixel margin.
  _isChevronButtonEnabled = _snippetLabelExpanded.frame.size.height -
                                _snippetLabelOneLine.frame.size.height >
                            1.;
  _chevronButton.alpha = (_isChevronButtonEnabled) ? 1. : .4;
}

#pragma mark - Properties

- (void)setChecked:(BOOL)checked {
  if (checked == _checked) {
    return;
  }
  _checked = checked;
  // `-[SnippetSearchEngineButton touchesEnded:withEvent:]` is called first,
  // which sets the background color with the animation. Then the view
  // controller receives the target action, which calls
  // `-[SnippetSearchEngineButton setChecked:YES]`. During the `setChecked:`
  // call, if the background color is updated, the animation is canceled.
  // So if the background color is already set, there is no point to set it
  // again.
  if (_checked && !self.backgroundColor) {
    self.backgroundColor = GetCheckedBackgroundColor();
  } else if (!_checked) {
    self.backgroundColor = nil;
  }
  [self updateCircleImageView];
  [self updateAccessibilityTraits];
}

- (void)setFaviconImage:(UIImage*)faviconImage {
  CGSize faviconImageSize =
      CGSizeMake(kFaviconImageViewSize, kFaviconImageViewSize);
  ResizeImage(faviconImage, faviconImageSize, ProjectionMode::kAspectFit);
  _faviconImageView.image = faviconImage;
}

- (UIImage*)faviconImage {
  return _faviconImageView.image;
}

- (void)setSearchEngineName:(NSString*)name {
  _nameLabel.text = name;
  [self updateChevronIdentifier];
}

- (NSString*)searchEngineName {
  return _nameLabel.text;
}

- (void)setSnippetButtonState:(SnippetButtonState)snippetButtonState {
  // This method should be called only when being configured, before to be
  // added to the view. Therefore there should be no animation.
  [self updateCellWithSnippetSate:snippetButtonState animate:NO];
}

- (void)setSnippetText:(NSString*)snippetText {
  _snippetText = [snippetText copy];
  _snippetLabelExpanded.text = _snippetText;
  _snippetLabelOneLine.text = _snippetText;
}

- (void)setHorizontalSeparatorHidden:(BOOL)hidden {
  _horizontalSeparator.hidden = hidden;
}

- (BOOL)horizontalSeparatorHidden {
  return _horizontalSeparator.hidden;
}

#pragma mark - Private

// Called by the chevron button.
- (void)chevronToggleAction:(id)sender {
  if (!_isChevronButtonEnabled) {
    return;
  }
  switch (_snippetButtonState) {
    case SnippetButtonState::kExpanded: {
      [self updateCellWithSnippetSate:SnippetButtonState::kOneLine animate:YES];
      NSString* collapsedFeedback = l10n_util::GetNSString(
          IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_SNIPPET_COLLAPSED);
      UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                      collapsedFeedback);
      break;
    }
    case SnippetButtonState::kOneLine: {
      [self updateCellWithSnippetSate:SnippetButtonState::kExpanded
                              animate:YES];
      UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                      self.snippetText);
      break;
    }
  }
}

// Updates the UI according to the new snippet state.
- (void)updateCellWithSnippetSate:(SnippetButtonState)newSnippetButtonState
                          animate:(BOOL)animate {
  // Need to avoid `if (_snippetButtonState == newSnippetButtonState) return;`,
  // so this method can be used by init method to setup the cell.
  _snippetButtonState = newSnippetButtonState;
  const float downRotation = 0;
  const float upRotation = downRotation + M_PI;
  UIButton* chevronButton = _chevronButton;
  UILabel* snippetLabelOneLine = _snippetLabelOneLine;
  UILabel* snippetLabelExpanded = _snippetLabelExpanded;
  NSLayoutConstraint* snippetLabelOneLineConstraint =
      _snippetLabelOneLineConstraint;
  NSLayoutConstraint* snippetLabelExpandedConstraint =
      _snippetLabelExpandedConstraint;
  [self updateChevronIdentifier];
  ProceduralBlock changesBlock = ^{
    switch (newSnippetButtonState) {
      case SnippetButtonState::kOneLine:
        chevronButton.transform =
            CGAffineTransformRotate(CGAffineTransformIdentity, downRotation);
        snippetLabelOneLine.alpha = 1;
        snippetLabelExpanded.alpha = 0;
        snippetLabelOneLineConstraint.active = YES;
        snippetLabelExpandedConstraint.active = NO;
        break;
      case SnippetButtonState::kExpanded:
        chevronButton.transform =
            CGAffineTransformRotate(CGAffineTransformIdentity, upRotation);
        snippetLabelOneLine.alpha = 0;
        snippetLabelExpanded.alpha = 1;
        snippetLabelOneLineConstraint.active = NO;
        snippetLabelExpandedConstraint.active = YES;
        break;
    }
    if (animate) {
      // Layout all the view to have a smooth transition.
      [self.animatedLayoutView layoutIfNeeded];
    }
  };
  if (animate) {
    [UIView animateWithDuration:kSnippetAnimationDurationInSecond
                     animations:changesBlock];
  } else {
    changesBlock();
  }
}

// Updates `_radioButtonImageView` based on `_checked`.
- (void)updateCircleImageView {
  UIImage* circleImage;
  if (_checked) {
    circleImage = DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                             kRadioButtonImageSize);
    _radioButtonImageView.tintColor = GetCheckedTintColor();
  } else {
    circleImage =
        DefaultSymbolWithPointSize(kCircleSymbol, kRadioButtonImageSize);
    _radioButtonImageView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  }
  _radioButtonImageView.image = circleImage;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  CHECK_NE(self.snippetText.length, 0ul, base::NotFatalUntil::M127)
      << base::SysNSStringToUTF8(self.searchEngineName) << " "
      << base::SysNSStringToUTF8(self.snippetText);
  return [NSString
      stringWithFormat:@"%@. %@", self.searchEngineName, self.snippetText];
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  CHECK_NE(self.searchEngineName.length, 0ul, base::NotFatalUntil::M127)
      << base::SysNSStringToUTF8(self.searchEngineName) << " "
      << base::SysNSStringToUTF8(self.snippetText);
  return @[ self.searchEngineName ];
}

- (NSString*)accessibilityIdentifier {
  return
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 self.searchEngineName];
}

- (BOOL)isAccessibilityElement {
  return YES;
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  if (!_isChevronButtonEnabled) {
    return [super accessibilityCustomActions];
  }
  NSString* actionName = nil;
  switch (_snippetButtonState) {
    case SnippetButtonState::kOneLine:
      actionName = l10n_util::GetNSString(
          IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_EXPAND_SNIPPET);
      break;
    case SnippetButtonState::kExpanded:
      actionName = l10n_util::GetNSString(
          IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_COLLAPSE_SNIPPET);
      break;
  }
  UIAccessibilityCustomAction* action = [[UIAccessibilityCustomAction alloc]
      initWithName:actionName
            target:self
          selector:@selector(chevronToggleAction:)];
  NSArray<UIAccessibilityCustomAction*>* actions = @[ action ];
  return actions;
}

- (void)updateChevronIdentifier {
  switch (_snippetButtonState) {
    case SnippetButtonState::kOneLine:
      _chevronButton.accessibilityIdentifier = [NSString
          stringWithFormat:@"%@%@",
                           kSnippetSearchEngineOneLineChevronIdentifierPrefix,
                           self.searchEngineName];
      break;
    case SnippetButtonState::kExpanded:
      base::RecordAction(
          base::UserMetricsAction(kExpandSearchEngineDescriptionUserAction));
      _chevronButton.accessibilityIdentifier = [NSString
          stringWithFormat:@"%@%@",
                           kSnippetSearchEngineExpandedChevronIdentifierPrefix,
                           self.searchEngineName];
      break;
  }
}

@end
