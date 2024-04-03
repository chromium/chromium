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
// The size of the radio button size.
constexpr CGFloat kRadioButtonSize = 24.;
// The size of the radio button image.
constexpr CGFloat kRadioButtonImageSize = 20.;
// Upper vertical margin for name label in the button.
constexpr CGFloat kUpperVerticalMargin = 10.;
// Lower vertical margine for the snippet label in the button.
constexpr CGFloat kLowerVerticalMargin = 10.;
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

}  // namespace

@implementation SnippetSearchEngineButton {
  // Container View for the faviconView.
  UIView* _faviconContainerView;
  UIImageView* _faviconImageView;
  UILabel* _nameLabel;
  SnippetButtonState _snippetButtonState;
  UIButton* _chevronButton;
  UIImageView* _radioButtonImageView;
  // Horizontal separator, shown only if `horizontalSeparatorHidden` is NO.
  UIView* _horizontalSeparator;
  // UILabel to display the first line of the snippet.
  UILabel* _snippetLabelOneLine;
  // UILabel to display the snippet with all lines.
  UILabel* _snippetLabelExpanded;
  // Constraint to display `_snippetLabelOneLine`
  NSLayoutConstraint* _snippetLabelOneLineConstraint;
  // Constraint to display `_snippetLabelExpanded`.
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
    // Add name label and snippet label.
    _nameLabel = [[UILabel alloc] init];
    _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _nameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _nameLabel.adjustsFontForContentSizeCategory = YES;
    [self addSubview:_nameLabel];
    // Make sure iOS prefers to strech the margins and not the name label,
    // by increasing the hugging priority.
    [_nameLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                                  forAxis:UILayoutConstraintAxisVertical];
    // Add snippet container that contains `_snippetLabelOneLine`, and
    // `_snippetLabelExpanded`.
    UIView* snippetLabelContainer = [[UIView alloc] init];
    snippetLabelContainer.userInteractionEnabled = NO;
    snippetLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:snippetLabelContainer];
    _snippetLabelOneLine = SnippetLabel();
    _snippetLabelOneLine.numberOfLines = 1;
    // Make sure the snippet is not streched.
    [_snippetLabelOneLine
        setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                          forAxis:UILayoutConstraintAxisVertical];
    [snippetLabelContainer addSubview:_snippetLabelOneLine];
    _snippetLabelExpanded = SnippetLabel();
    _snippetLabelExpanded.numberOfLines = 0;
    // Make sure the snippet is not streched.
    [_snippetLabelExpanded
        setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                          forAxis:UILayoutConstraintAxisVertical];
    [snippetLabelContainer addSubview:_snippetLabelExpanded];
    // Add Chevron.
    _chevronButton = [[UIButton alloc] init];
    _chevronButton.translatesAutoresizingMaskIntoConstraints = NO;
    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    configuration.image = DefaultSymbolTemplateWithPointSize(
        kChevronDownSymbol, kSymbolAccessoryPointSize);
    _chevronButton.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
    _chevronButton.configuration = configuration;
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
    // Add constraints.
    // Constraint when the snippet is expanded.
    _snippetLabelExpandedConstraint = [snippetLabelContainer.bottomAnchor
        constraintEqualToAnchor:_snippetLabelOneLine.bottomAnchor];
    _snippetLabelExpandedConstraint.priority = UILayoutPriorityDefaultHigh;
    // Constraint when the snippet is only one line.
    _snippetLabelOneLineConstraint = [snippetLabelContainer.bottomAnchor
        constraintEqualToAnchor:_snippetLabelExpanded.bottomAnchor];
    _snippetLabelOneLineConstraint.priority = UILayoutPriorityDefaultLow;
    NSArray* constraints = @[
      // Favicon and favicon container.
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kBorderHorizontalMargin],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
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
      // Name label.
      [_nameLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kUpperVerticalMargin],
      [_nameLabel.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kInnerHorizontalMargin],
      [_nameLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_chevronButton.leadingAnchor
                                   constant:-kChevronButtonHorizontalMargin],
      [_nameLabel.bottomAnchor
          constraintEqualToAnchor:snippetLabelContainer.topAnchor],
      // Snippet labels and container.
      [snippetLabelContainer.leadingAnchor
          constraintEqualToAnchor:_nameLabel.leadingAnchor],
      [snippetLabelContainer.trailingAnchor
          constraintLessThanOrEqualToAnchor:_chevronButton.leadingAnchor
                                   constant:-kChevronButtonHorizontalMargin],
      [snippetLabelContainer.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kLowerVerticalMargin],
      [snippetLabelContainer.topAnchor
          constraintEqualToAnchor:_snippetLabelExpanded.topAnchor],
      [snippetLabelContainer.leadingAnchor
          constraintEqualToAnchor:_snippetLabelExpanded.leadingAnchor],
      [snippetLabelContainer.trailingAnchor
          constraintEqualToAnchor:_snippetLabelExpanded.trailingAnchor],
      [snippetLabelContainer.topAnchor
          constraintEqualToAnchor:_snippetLabelOneLine.topAnchor],
      [snippetLabelContainer.leadingAnchor
          constraintEqualToAnchor:_snippetLabelOneLine.leadingAnchor],
      [snippetLabelContainer.trailingAnchor
          constraintEqualToAnchor:_snippetLabelOneLine.trailingAnchor],
      _snippetLabelExpandedConstraint,
      _snippetLabelOneLineConstraint,
      // Chevron.
      [_chevronButton.heightAnchor
          constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.widthAnchor constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_chevronButton.trailingAnchor
          constraintEqualToAnchor:verticalSeparator.leadingAnchor
                         constant:-kChevronButtonHorizontalMargin],
      // Vertical separator.
      [verticalSeparator.heightAnchor
          constraintEqualToAnchor:_nameLabel.heightAnchor],
      [verticalSeparator.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [verticalSeparator.widthAnchor
          constraintEqualToConstant:kSeparatorThickness],
      [verticalSeparator.trailingAnchor
          constraintEqualToAnchor:_radioButtonImageView.leadingAnchor
                         constant:-kInnerHorizontalMargin],
      // Radio button.
      [_radioButtonImageView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_radioButtonImageView.widthAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.heightAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kBorderHorizontalMargin],
      // HorizontalSeparator.
      [_horizontalSeparator.leadingAnchor
          constraintEqualToAnchor:_nameLabel.leadingAnchor],
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
  self.backgroundColor = [UIColor colorNamed:kGrey300Color];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:.5
                   animations:^{
                     weakSelf.backgroundColor = nil;
                   }];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  self.backgroundColor = nil;
}

#pragma mark - Properties

- (void)setChecked:(BOOL)checked {
  if (checked == _checked) {
    return;
  }
  _checked = checked;
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
  UIView* chevronButton = _chevronButton;
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
        snippetLabelOneLineConstraint.priority = UILayoutPriorityDefaultLow;
        snippetLabelExpandedConstraint.priority = UILayoutPriorityDefaultHigh;
        break;
      case SnippetButtonState::kExpanded:
        chevronButton.transform =
            CGAffineTransformRotate(CGAffineTransformIdentity, upRotation);
        snippetLabelOneLine.alpha = 0;
        snippetLabelExpanded.alpha = 1;
        snippetLabelOneLineConstraint.priority = UILayoutPriorityDefaultHigh;
        snippetLabelExpandedConstraint.priority = UILayoutPriorityDefaultLow;
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
    [_radioButtonImageView setTintColor:[UIColor colorNamed:kBlueColor]];
  } else {
    circleImage =
        DefaultSymbolWithPointSize(kCircleSymbol, kRadioButtonImageSize);
    [_radioButtonImageView
        setTintColor:[UIColor colorNamed:kTextQuaternaryColor]];
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
