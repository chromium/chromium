// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/snippet_search_engine_button.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/search_engine_choice/ui/constants.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_constants.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_current_default_pill_view.h"
#import "ios/chrome/browser/search_engine_choice/ui/snippet_search_engine_text_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

// List of values according to the style.
typedef struct {
  // Upper vertical margin for name label in the button.
  const CGFloat upper_vertical_margin;
  // Lower vertical margin for the snippet label in the button.
  const CGFloat lower_vertical_margin;
} ButtonLayout;

// Style with no current default pill.
const ButtonLayout kNoCurrentDefaultButtonLayout = {
    10,  // upper_vertical_margin
    12,  // lower_vertical_margin
};

// Style with current default pill visible or hidden.
const ButtonLayout kWithCurrentDefaultButtonLayout = {
    8,  // upper_vertical_margin
    5,  // lower_vertical_margin
};

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
  UIButton* _chevronButton;
  BOOL _isChevronButtonEnabled;
  UIImageView* _radioButtonImageView;
  // Horizontal separator, shown only if `horizontalSeparatorHidden` is NO.
  UIView* _horizontalSeparator;
  SnippetSearchEngineTextView* _textView;
}

- (instancetype)initWithCurrentDefaultState:
    (SearchEngineCurrentDefaultState)currentDefaultState {
  self = [super initWithFrame:CGRectZero];
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
    _textView = [[SnippetSearchEngineTextView alloc]
        initWithCurrentDefaultState:currentDefaultState];
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.userInteractionEnabled = NO;
    [self addSubview:_textView];
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

    const ButtonLayout* buttonLayout = nil;
    switch (currentDefaultState) {
      case SearchEngineCurrentDefaultState::kNone:
        buttonLayout = &kNoCurrentDefaultButtonLayout;
        break;
      case SearchEngineCurrentDefaultState::kOtherIsDefault:
        buttonLayout = &kWithCurrentDefaultButtonLayout;
        break;
      case SearchEngineCurrentDefaultState::kIsDefault:
        buttonLayout = &kWithCurrentDefaultButtonLayout;
        break;
    }
    CHECK(buttonLayout);
    NSArray* constraints = @[
      // Constraints for avicon and favicon container.
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kBorderHorizontalMargin],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:_textView.oneLineCenterVerticalLayoutGuide],
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
      [_textView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:buttonLayout->upper_vertical_margin],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-buttonLayout->lower_vertical_margin],
      [_textView.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kInnerHorizontalMargin],
      [_textView.trailingAnchor
          constraintEqualToAnchor:_chevronButton.leadingAnchor
                         constant:-kChevronButtonHorizontalMargin],
      // Constraints for chevron.
      [_chevronButton.heightAnchor
          constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.widthAnchor constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.centerYAnchor
          constraintEqualToAnchor:_textView.oneLineCenterVerticalLayoutGuide],
      [_chevronButton.trailingAnchor
          constraintEqualToAnchor:verticalSeparator.leadingAnchor
                         constant:-kChevronButtonHorizontalMargin],
      // Constraints for vertical separator.
      [verticalSeparator.heightAnchor
          constraintEqualToAnchor:_textView.searchEngineNameLabelHeight],
      [verticalSeparator.centerYAnchor
          constraintEqualToAnchor:_textView.oneLineCenterVerticalLayoutGuide],
      [verticalSeparator.widthAnchor
          constraintEqualToConstant:kSeparatorThickness],
      [verticalSeparator.trailingAnchor
          constraintEqualToAnchor:_radioButtonImageView.leadingAnchor
                         constant:-kInnerHorizontalMargin],
      // Constraints for radio button.
      [_radioButtonImageView.centerYAnchor
          constraintEqualToAnchor:_textView.oneLineCenterVerticalLayoutGuide],
      [_radioButtonImageView.widthAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.heightAnchor
          constraintEqualToConstant:kRadioButtonSize],
      [_radioButtonImageView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kBorderHorizontalMargin],
      // Constraints for horizontal separator.
      [_horizontalSeparator.leadingAnchor
          constraintEqualToAnchor:_textView.leadingAnchor],
      [_horizontalSeparator.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_horizontalSeparator.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_horizontalSeparator.heightAnchor
          constraintEqualToConstant:kSeparatorThickness],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
    [self updateCellWithExpanded:NO animate:NO];
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
  BOOL isExandable = _textView.isSnippetExpandable;
  if (!isExandable && _snippetExpanded) {
    // The snippet is now on line, the view should be updated accordingly.
    [self updateCellWithExpanded:NO animate:NO];
  }
  _isChevronButtonEnabled = _textView.isSnippetExpandable;
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
  _textView.searchEngineName = name;
  [self updateChevronIdentifier];
}

- (NSString*)searchEngineName {
  return _textView.searchEngineName;
}

- (void)setExpanded:(BOOL)expanded {
  // This method should be called only when being configured, before to be
  // added to the view. Therefore there should be no animation.
  [self updateCellWithExpanded:expanded animate:NO];
}

- (void)setSnippetText:(NSString*)snippetText {
  _textView.snippetText = snippetText;
}

- (NSString*)snippetText {
  return _textView.snippetText;
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
  [self updateCellWithExpanded:!_snippetExpanded animate:YES];
  if (_snippetExpanded) {
    NSString* collapsedFeedback = l10n_util::GetNSString(
        IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_SNIPPET_COLLAPSED);
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    collapsedFeedback);
  } else {
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    self.snippetText);
  }
}

// Updates the UI according to the new snippet state.
- (void)updateCellWithExpanded:(BOOL)newSnippetExpanded animate:(BOOL)animate {
  // Need to avoid `if (_snippetExpanded == newSnippetExpanded) return;`, so
  // this method can be used by init method to setup the cell.
  _snippetExpanded = newSnippetExpanded;
  const float downRotation = 0;
  const float upRotation = downRotation + M_PI;
  UIButton* chevronButton = _chevronButton;
  SnippetSearchEngineTextView* textView = _textView;
  [self updateChevronIdentifier];
  ProceduralBlock changesBlock = ^{
    if (newSnippetExpanded) {
      chevronButton.transform =
          CGAffineTransformRotate(CGAffineTransformIdentity, upRotation);
    } else {
      chevronButton.transform =
          CGAffineTransformRotate(CGAffineTransformIdentity, downRotation);
    }
    textView.snippetExpanded = newSnippetExpanded;
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

- (void)updateChevronIdentifier {
  if (_snippetExpanded) {
    base::RecordAction(
        base::UserMetricsAction(kExpandSearchEngineDescriptionUserAction));
    _chevronButton.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@",
                         kSnippetSearchEngineExpandedChevronIdentifierPrefix,
                         self.searchEngineName];
  } else {
    _chevronButton.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@",
                         kSnippetSearchEngineOneLineChevronIdentifierPrefix,
                         self.searchEngineName];
  }
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  CHECK_NE(self.searchEngineName.length, 0ul)
      << base::SysNSStringToUTF8(self.searchEngineKeyword);
  CHECK_NE(self.snippetText.length, 0ul)
      << base::SysNSStringToUTF8(self.searchEngineKeyword);
  switch (_textView.currentDefaultState) {
    case SearchEngineCurrentDefaultState::kNone:
    case SearchEngineCurrentDefaultState::kOtherIsDefault:
      return l10n_util::GetNSStringF(
          IDS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_VOICEOVER,
          base::SysNSStringToUTF16(self.searchEngineName),
          base::SysNSStringToUTF16(self.snippetText));
    case SearchEngineCurrentDefaultState::kIsDefault:
      return l10n_util::GetNSStringF(
          IDS_SEARCH_ENGINE_CHOICE_CURRENT_DEFAULT_SEARCH_ENGINE_VOICEOVER,
          base::SysNSStringToUTF16(self.searchEngineName),
          base::SysNSStringToUTF16(self.snippetText));
  }
  NOTREACHED();
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  CHECK_NE(self.searchEngineName.length, 0ul)
      << base::SysNSStringToUTF8(self.searchEngineName) << " "
      << base::SysNSStringToUTF8(self.snippetText);
  return @[ self.searchEngineName ];
}

- (BOOL)isAccessibilityElement {
  return YES;
}

#pragma mark - UIAccessibilityIdentification

- (NSString*)accessibilityIdentifier {
  return
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 self.searchEngineName];
}

#pragma mark - UIAccessibilityAction

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  if (!_isChevronButtonEnabled) {
    return [super accessibilityCustomActions];
  }
  NSString* actionName = nil;
  if (_snippetExpanded) {
    actionName = l10n_util::GetNSStringF(
        IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_COLLAPSE_SNIPPET,
        base::SysNSStringToUTF16(self.searchEngineName));
  } else {
    actionName = l10n_util::GetNSStringF(
        IDS_IOS_SEARCH_ENGINE_ACCESSIBILITY_EXPAND_SNIPPET,
        base::SysNSStringToUTF16(self.searchEngineName));
  }
  UIAccessibilityCustomAction* action = [[UIAccessibilityCustomAction alloc]
      initWithName:actionName
            target:self
          selector:@selector(chevronToggleAction:)];
  NSArray<UIAccessibilityCustomAction*>* actions = @[ action ];
  return actions;
}

@end
