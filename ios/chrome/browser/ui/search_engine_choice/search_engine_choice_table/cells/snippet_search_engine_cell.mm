// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The size of the radio button at the side of each cell.
constexpr CGFloat kRadioButtonSize = 22.;
// Vertical margin for the elements in SnippetSearchEngineCell.
constexpr CGFloat kVerticalMargin = 17.;
// Horizontal margin between elements in SnippetSearchEngineCell.
constexpr CGFloat kInnerHorizontalMargin = 12.;
// Chevron button size.
constexpr CGFloat kChevronButtonSize = 24.;
// Thickness of the vertical separator.
constexpr CGFloat kSeparatorThickness = 1.;
// Duration of the snippet animation when changing state.
constexpr NSTimeInterval kSnippetAnimationDurationInSecond = .3;

}  // namespace

@implementation SnippetSearchEngineCell {
  // Container View for the faviconView.
  FaviconContainerView* _faviconContainerView;
  NSLayoutConstraint* _showSnippetConstraint;
  NSLayoutConstraint* _hiddenSnippetConstraint;
  SnippetState _snippetState;
  UIButton* _chevronButton;
  UIImageView* _checkedCircleImageView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    UIView* contentView = self.contentView;
    contentView.clipsToBounds = YES;
    _faviconContainerView = [[FaviconContainerView alloc] init];
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [_faviconContainerView
        setFaviconBackgroundColor:[UIColor colorNamed:kBackgroundColor]];
    [contentView addSubview:_faviconContainerView];
    [_faviconContainerView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    // Add name label and snippet label.
    _nameLabel = [[UILabel alloc] init];
    _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _nameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _nameLabel.adjustsFontForContentSizeCategory = YES;
    [contentView addSubview:_nameLabel];
    // Make sure iOS prefers to strech the margins and not the name label,
    // by increasing the hugging priority.
    [_nameLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                                  forAxis:UILayoutConstraintAxisVertical];
    _snippetLabel = [[UILabel alloc] init];
    _snippetLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _snippetLabel.adjustsFontForContentSizeCategory = YES;
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.numberOfLines = 0;
    [contentView addSubview:_snippetLabel];
    // Add Chevron.
    _chevronButton = [[UIButton alloc] init];
    _chevronButton.translatesAutoresizingMaskIntoConstraints = NO;
    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    configuration.image = DefaultSymbolTemplateWithPointSize(
        kChevronDownSymbol, kSymbolAccessoryPointSize);
    _chevronButton.tintColor = [UIColor colorNamed:kGrey700Color];
    _chevronButton.configuration = configuration;
    [_chevronButton addTarget:self
                       action:@selector(chevronToggleAction:)
             forControlEvents:UIControlEventTouchUpInside];
    [contentView addSubview:_chevronButton];
    // Add separator.
    UIView* separatorLine = [[UIView alloc] init];
    separatorLine.translatesAutoresizingMaskIntoConstraints = NO;
    separatorLine.backgroundColor = [UIColor colorNamed:kGrey700Color];
    [contentView addSubview:separatorLine];
    // Add the checked circle holder.
    _checkedCircleImageView = [[UIImageView alloc] init];
    _checkedCircleImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_checkedCircleImageView];
    [_checkedCircleImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    // Add constraints
    _hiddenSnippetConstraint = [_nameLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kVerticalMargin];
    _hiddenSnippetConstraint.priority = UILayoutPriorityDefaultHigh;
    _showSnippetConstraint = [_snippetLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kVerticalMargin];
    _showSnippetConstraint.priority = UILayoutPriorityDefaultLow;
    NSArray* constraints = @[
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:_nameLabel.centerYAnchor],
      [_nameLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kVerticalMargin],
      [_nameLabel.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kVerticalMargin],
      [_nameLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_chevronButton.leadingAnchor
                                   constant:-kInnerHorizontalMargin],
      [_nameLabel.bottomAnchor constraintEqualToAnchor:_snippetLabel.topAnchor],
      [_snippetLabel.leadingAnchor
          constraintEqualToAnchor:_nameLabel.leadingAnchor],
      [_snippetLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_chevronButton.leadingAnchor
                                   constant:-kInnerHorizontalMargin],
      [_chevronButton.heightAnchor
          constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.widthAnchor constraintEqualToConstant:kChevronButtonSize],
      [_chevronButton.centerYAnchor
          constraintEqualToAnchor:_nameLabel.centerYAnchor],
      [_chevronButton.trailingAnchor
          constraintEqualToAnchor:separatorLine.leadingAnchor
                         constant:-kInnerHorizontalMargin],
      [separatorLine.heightAnchor
          constraintEqualToAnchor:_nameLabel.heightAnchor],
      [separatorLine.centerYAnchor
          constraintEqualToAnchor:_nameLabel.centerYAnchor],
      [separatorLine.widthAnchor constraintEqualToConstant:kSeparatorThickness],
      [separatorLine.trailingAnchor
          constraintEqualToAnchor:_checkedCircleImageView.leadingAnchor
                         constant:-kInnerHorizontalMargin],
      [_checkedCircleImageView.centerYAnchor
          constraintEqualToAnchor:_nameLabel.centerYAnchor],
      [_checkedCircleImageView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      _hiddenSnippetConstraint,
      _showSnippetConstraint,
    ];
    [NSLayoutConstraint activateConstraints:constraints];
    [self updateCellWithSnippetSate:SnippetState::kHidden animate:NO];
    [self updateCircleImageView];
  }
  return self;
}

#pragma mark - Properties

- (FaviconView*)faviconView {
  return _faviconContainerView.faviconView;
}

- (void)setChecked:(BOOL)checked {
  if (checked == _checked) {
    return;
  }
  _checked = checked;
  [self updateCircleImageView];
}

#pragma mark - Private

// Called by the chevron button.
- (void)chevronToggleAction:(id)sender {
  switch (_snippetState) {
    case SnippetState::kShown:
      [self updateCellWithSnippetSate:SnippetState::kHidden animate:YES];
      break;
    case SnippetState::kHidden:
      [self updateCellWithSnippetSate:SnippetState::kShown animate:YES];
      break;
  }
  if (self.chevronToggledBlock) {
    self.chevronToggledBlock(_snippetState);
  }
}

// Updates the UI according to the new snippet state.
- (void)updateCellWithSnippetSate:(SnippetState)newSnippetState
                          animate:(BOOL)animate {
  // Need to avoid `if (_snippetState == newSnippetState) return;`, so this
  // method can be used by init method to setup the cell.
  _snippetState = newSnippetState;
  const float downRotation = 0;
  const float upRotation = downRotation + M_PI;
  CGFloat angle;
  CGFloat snippetLabelAlpha;
  UILayoutPriority showSnippetConstraintPriority;
  UILayoutPriority hiddenSnippetConstraintPriority;
  switch (_snippetState) {
    case SnippetState::kHidden:
      angle = upRotation;
      snippetLabelAlpha = 0;
      showSnippetConstraintPriority = UILayoutPriorityDefaultLow;
      hiddenSnippetConstraintPriority = UILayoutPriorityDefaultHigh;
      break;
    case SnippetState::kShown:
      angle = downRotation;
      snippetLabelAlpha = 1;
      showSnippetConstraintPriority = UILayoutPriorityDefaultHigh;
      hiddenSnippetConstraintPriority = UILayoutPriorityDefaultLow;
      break;
  }
  UIView* chevronButton = _chevronButton;
  UIView* snippetLabel = _snippetLabel;
  NSLayoutConstraint* showSnippetConstraint = _showSnippetConstraint;
  NSLayoutConstraint* hiddenSnippetConstraint = _hiddenSnippetConstraint;
  ProceduralBlock changesBlock = ^{
    chevronButton.transform =
        CGAffineTransformRotate(CGAffineTransformIdentity, angle);
    snippetLabel.alpha = snippetLabelAlpha;
    showSnippetConstraint.priority = showSnippetConstraintPriority;
    hiddenSnippetConstraint.priority = hiddenSnippetConstraintPriority;
  };
  if (animate) {
    [UIView animateWithDuration:kSnippetAnimationDurationInSecond
                     animations:changesBlock];
  } else {
    changesBlock();
  }
}

// Updates `_checkedCircleImageView` based on `_checked`.
- (void)updateCircleImageView {
  UIImage* circleImage;
  if (_checked) {
    circleImage = DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                             kRadioButtonSize);
    [_checkedCircleImageView setTintColor:[UIColor colorNamed:kBlue500Color]];
  } else {
    circleImage = DefaultSymbolWithPointSize(kCircleSymbol, kRadioButtonSize);
    [_checkedCircleImageView setTintColor:[UIColor colorNamed:kGrey700Color]];
  }
  _checkedCircleImageView.image = circleImage;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.faviconView configureWithAttributes:nil];
  self.chevronToggledBlock = nil;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  NSString* accessibilityLabel = self.nameLabel.text;
  if (self.snippetLabel.text.length > 0) {
    accessibilityLabel = [NSString
        stringWithFormat:@"%@. %@", accessibilityLabel, self.snippetLabel.text];
  }
  return accessibilityLabel;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  CHECK(self.nameLabel.text);
  return @[ self.nameLabel.text ];
}

- (NSString*)accessibilityIdentifier {
  return
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 self.nameLabel.text];
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end
