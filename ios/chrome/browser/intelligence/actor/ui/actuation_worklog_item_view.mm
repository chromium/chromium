// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Spacing values
const CGFloat kSpacingTiny = 4.0;
const CGFloat kSpacingSmall = 8.0;
const CGFloat kSpacingMedium = 12.0;
const CGFloat kSpacingLarge = 16.0;

const CGFloat kTimelineGutterWidth = 50.0;

const CGFloat kDashLength = 6.0;
const CGFloat kConnectorLineWidth = 2.0;
const CGFloat kDotBorderWidth = 2.0;
const CGFloat kDotSizeSimple = 8.0;
const CGFloat kDotSizeLabeled = 32.0;

const CGFloat kIconSize = 16.0;

}  // namespace

@implementation ActuationWorklogItemView {
  UIView* _dotView;
  UIImageView* _iconView;
  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UIStackView* _mainRowStack;

  NSLayoutConstraint* _dotSizeConstraint;

  BOOL _active;
  ActuationWorklogItemStyle _style;
  CAShapeLayer* _connectorLayer;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _connectorVisibility = ActuationWorklogConnectorVisibility::kNone;

    _connectorLayer = [CAShapeLayer layer];
    _connectorLayer.strokeColor = [UIColor colorNamed:kGrey400Color].CGColor;
    _connectorLayer.lineWidth = kConnectorLineWidth;
    _connectorLayer.lineDashPattern = @[ @(kDashLength), @(kDashLength) ];
    [self.layer insertSublayer:_connectorLayer atIndex:0];

    _dotView = [[UIView alloc] init];
    _dotView.translatesAutoresizingMaskIntoConstraints = NO;
    _dotView.clipsToBounds = YES;
    _dotView.layer.borderColor = [UIColor colorNamed:kGrey400Color].CGColor;
    [self addSubview:_dotView];

    _iconView = [[UIImageView alloc] init];
    _iconView.contentMode = UIViewContentModeScaleAspectFit;
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.tintColor = [UIColor colorNamed:kGrey600Color];
    [_dotView addSubview:_iconView];

    _mainRowStack = [[UIStackView alloc] init];
    _mainRowStack.axis = UILayoutConstraintAxisVertical;
    _mainRowStack.alignment = UIStackViewAlignmentFill;
    _mainRowStack.layoutMarginsRelativeArrangement = YES;
    _mainRowStack.clipsToBounds = YES;
    _mainRowStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_mainRowStack];

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_mainRowStack addArrangedSubview:_titleLabel];

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_mainRowStack addArrangedSubview:_subtitleLabel];

    [self setupConstraints];
  }
  return self;
}

- (void)configureWithItem:(ActuationWorklogItem*)item {
  _active = item.isActive;
  _style = item.style;

  [self updateContentFromItem:item];
  [self updateCardStyleAndLayout];
  [self updateFontsAndColors];
  [self updateDotAppearance];
}

- (void)setConnectorVisibility:
    (ActuationWorklogConnectorVisibility)connectorVisibility {
  if (_connectorVisibility != connectorVisibility) {
    _connectorVisibility = connectorVisibility;
    [self setNeedsLayout];
  }
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust layers since CGColor does not automatically update when the system
  // switches between light and dark modes.
  UIColor* grey400 = [UIColor colorNamed:kGrey400Color];
  _connectorLayer.strokeColor = grey400.CGColor;
  _dotView.layer.borderColor = grey400.CGColor;
  [self updateConnectorPath];
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  // Force layout when attached to a superview to ensure the connector phase
  // is correctly calculated using the parent container's coordinate space.
  [self setNeedsLayout];
}

#pragma mark - Private

// Updates the connector line's shape path and dash phase alignment.
- (void)updateConnectorPath {
  if (_connectorVisibility == ActuationWorklogConnectorVisibility::kNone) {
    _connectorLayer.path = nil;
    return;
  }

  CGFloat lineCenterX = kTimelineGutterWidth / 2.0;

  BOOL hasTopLine =
      (_connectorVisibility == ActuationWorklogConnectorVisibility::kBoth ||
       _connectorVisibility == ActuationWorklogConnectorVisibility::kTop);
  BOOL hasBottomLine =
      (_connectorVisibility == ActuationWorklogConnectorVisibility::kBottom ||
       _connectorVisibility == ActuationWorklogConnectorVisibility::kBoth);
  CGFloat start = hasTopLine ? 0.0 : _dotView.center.y;
  CGFloat end = hasBottomLine ? self.bounds.size.height : _dotView.center.y;

  // TODO(crbug.com/532209191): This will need to take into account the various
  // layout styles of the superviews (compact mode and full scrollable worklog)
  // Adjust dash phase to align with the parent container coordinate space
  // so that dashes across cells connect seamlessly without overlaps.
  UIView* parent = self.superview;
  if (parent) {
    CGPoint originInParent = [self convertPoint:CGPointZero toView:parent];
    CGFloat patternLength = kDashLength * 2;
    _connectorLayer.lineDashPhase =
        fmod(originInParent.y + start, patternLength);
  }

  UIBezierPath* path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(lineCenterX, start)];
  [path addLineToPoint:CGPointMake(lineCenterX, end)];
  _connectorLayer.path = path.CGPath;
}

// Setup layout constraints.
- (void)setupConstraints {
  _dotSizeConstraint =
      [_dotView.widthAnchor constraintEqualToConstant:kDotSizeSimple];

  // Center dot within the gutter, and align with the title label.
  [NSLayoutConstraint activateConstraints:@[
    [_dotView.centerXAnchor constraintEqualToAnchor:self.leadingAnchor
                                           constant:kTimelineGutterWidth / 2.0],
    [_dotView.centerYAnchor constraintEqualToAnchor:_titleLabel.centerYAnchor],
    [_dotView.heightAnchor constraintEqualToAnchor:_dotView.widthAnchor],
    _dotSizeConstraint,
  ]];

  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      kSpacingTiny, kTimelineGutterWidth, kSpacingTiny, kSpacingLarge);
  AddSameConstraintsWithInsets(_mainRowStack, self, insets);
  AddSameCenterConstraints(_iconView, _dotView);
  AddSquareConstraints(_iconView, kIconSize);
}

// Updates the string and images of subviews along with their visibility.
- (void)updateContentFromItem:(ActuationWorklogItem*)item {
  _titleLabel.text = item.title;

  BOOL showSubtitle =
      _style != ActuationWorklogItemStyle::kSimple && item.subtitle.length > 0;
  _subtitleLabel.hidden = !showSubtitle;
  _subtitleLabel.text = showSubtitle ? item.subtitle : nil;

  BOOL hasIcon = (_style != ActuationWorklogItemStyle::kSimple) && item.icon;
  _iconView.image =
      hasIcon ? [item.icon
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
              : nil;
  _iconView.hidden = !hasIcon;
}

// Updates constraints values and spacing based on the view style.
- (void)updateCardStyleAndLayout {
  self.backgroundColor = [UIColor clearColor];

  BOOL isCard = (_style == ActuationWorklogItemStyle::kCard);

  _mainRowStack.backgroundColor =
      isCard ? [UIColor colorNamed:kSecondaryBackgroundColor]
             : [UIColor clearColor];
  _mainRowStack.layer.cornerRadius = isCard ? kSpacingLarge : 0.0;

  CGFloat verticalPadding = isCard ? kSpacingMedium : kSpacingSmall;
  CGFloat horizontalPadding = isCard ? kSpacingLarge : 0.0;
  _mainRowStack.layoutMargins = UIEdgeInsetsMake(
      verticalPadding, horizontalPadding, verticalPadding, horizontalPadding);

  CGFloat spacing = isCard ? kSpacingSmall : kSpacingTiny;
  [_mainRowStack setCustomSpacing:spacing afterView:_titleLabel];
}

// Updates the font and color based on the view style.
- (void)updateFontsAndColors {
  BOOL simpleStyle = (_style == ActuationWorklogItemStyle::kSimple);
  _titleLabel.font =
      simpleStyle
          ? [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
          : CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  _titleLabel.textColor = simpleStyle ? [UIColor colorNamed:kTextSecondaryColor]
                                      : [UIColor colorNamed:kTextPrimaryColor];
}

// Updates the icon/dot appearance based on the view style.
- (void)updateDotAppearance {
  CGFloat dotSize;
  BOOL showLargeDot = (_style != ActuationWorklogItemStyle::kSimple);

  if (showLargeDot) {
    _dotView.backgroundColor = [UIColor colorNamed:kGrey200Color];
    dotSize = kDotSizeLabeled;
  } else {
    _dotView.backgroundColor = _active ? [UIColor colorNamed:kSolidWhiteColor]
                                       : [UIColor colorNamed:kGrey400Color];
    dotSize = _active ? (kDotSizeSimple + kDotBorderWidth) : kDotSizeSimple;
  }

  _dotView.layer.cornerRadius = dotSize / 2.0;
  _dotView.layer.borderWidth =
      (_active && !showLargeDot) ? kDotBorderWidth : 0.0;
  _dotSizeConstraint.constant = dotSize;
}

@end
