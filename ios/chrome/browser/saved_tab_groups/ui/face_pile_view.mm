// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_view.h"

#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between overlapping avatars.
const CGFloat kAvatarSpacing = -6;

// Substracted stroke around containers.
const CGFloat kContainerSubstractredStroke = 2;

// Font size of `plusXLabel`.
const CGFloat kPlusXlabelFontSize = 9;

// horizontal inner margin in for the `_plusXLabel`.
const CGFloat kPlusXlabelContainerHorizontalInnerMargin = 6;

// Alpha value of the non-avatar view background color.
const CGFloat kNonAvatarContainerBackgroundAlpha = 0.2;

// Proportion of the "person waiting" icon compared to the avatar size.
const CGFloat kPersonWaitingProportion = 0.55;

// Vertical margin for the "person waiting" icon.
const CGFloat kPersonWaitingVerticalMargin = 1.5;

}  // namespace

@implementation FacePileView {
  // Array of avatar primitive objects, each representing a face in the pile.
  NSArray<id<ShareKitAvatarPrimitive>>* _avatars;
  // A UIStackView to arrange and display the individual avatar views.
  UIStackView* _facesStackView;
  // Label to display text when the face pile is empty.
  UILabel* _emptyFacePileLabel;
  // Sets the background color for the face pile, visible in gaps and as an
  // outer stroke.
  UIColor* _facePileBackgroundColor;
  // Size of avatar faces, in points.
  CGFloat _avatarSize;
  // Container for the non-avatar element.
  UIView* _nonAvatarContainer;
  // Label to display the "+X" person in the group.
  UILabel* _plusXLabel;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.layer.masksToBounds = YES;
    _facesStackView = [[UIStackView alloc] init];
    _facesStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _facesStackView.spacing = kAvatarSpacing;
    [self addSubview:_facesStackView];
    AddSameConstraints(self, _facesStackView);

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateColors)];
  }
  return self;
}

#pragma mark - FacePileConsumer

- (void)setShowsTextWhenEmpty:(BOOL)showsTextWhenEmpty {
  if (showsTextWhenEmpty) {
    [self addEmptyFacePileLabel];
    _emptyFacePileLabel.hidden = ![self isEmpty];
  } else {
    [_emptyFacePileLabel removeFromSuperview];
    _emptyFacePileLabel = nil;
  }
}

- (void)setFacePileBackgroundColor:(UIColor*)backgroundColor {
  if (!backgroundColor) {
    return;
  }
  _facePileBackgroundColor = backgroundColor;
  self.backgroundColor = _facePileBackgroundColor;
}

- (void)updateWithFaces:(NSArray<id<ShareKitAvatarPrimitive>>*)faces
            totalNumber:(NSInteger)totalNumber {
  // Remove all existing avatar views from the stack.
  for (UIView* view in _facesStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  _avatars = faces;
  CGFloat containerSize = _avatarSize + kContainerSubstractredStroke * 2;
  NSInteger avatarsCount = static_cast<NSUInteger>(_avatars.count);

  for (NSInteger index = 0; index < avatarsCount; index++) {
    id<ShareKitAvatarPrimitive> avatar = _avatars[index];
    [avatar resolve];

    // Create a container view to act as the border for the avatar.
    // This is needed to avoid anti-aliasing artifacts around the border itself.
    UIView* avatarContainerView =
        [self createCircularContainerWithSize:containerSize];
    UIView* avatarView = [avatar view];
    avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    [avatarContainerView addSubview:avatarView];

    // Apply an overlapping mask to all avatar containers except if it's the
    // last displayed container.
    if (index + 1 < totalNumber || totalNumber == 1) {
      [self applyOverlappingCircleMaskToContainer:avatarContainerView];
    }

    [NSLayoutConstraint activateConstraints:@[
      [avatarContainerView.widthAnchor constraintEqualToConstant:containerSize],
      [avatarContainerView.heightAnchor constraintEqualToConstant:containerSize]
    ]];
    AddSameCenterConstraints(avatarView, avatarContainerView);

    [_facesStackView addArrangedSubview:avatarContainerView];
  }

  // Update the `_emptyFacePileLabel` visibility.
  _emptyFacePileLabel.hidden = ![self isEmpty];

  if (totalNumber == 1) {
    // Create a container view to act as the border.
    // This is needed to avoid anti-aliasing artifacts around the border itself.
    UIView* containerView =
        [self createCircularContainerWithSize:containerSize];
    containerView.layer.cornerRadius = containerSize / 2.0;

    [_facesStackView addArrangedSubview:containerView];

    [NSLayoutConstraint activateConstraints:@[
      [containerView.widthAnchor constraintEqualToConstant:containerSize],
      [containerView.heightAnchor constraintEqualToConstant:containerSize]
    ]];

    _nonAvatarContainer = [[UIView alloc] init];
    _nonAvatarContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _nonAvatarContainer.layer.cornerRadius = _avatarSize / 2.0;
    _nonAvatarContainer.layer.masksToBounds = YES;
    [NSLayoutConstraint activateConstraints:@[
      [_nonAvatarContainer.widthAnchor constraintEqualToConstant:_avatarSize],
      [_nonAvatarContainer.heightAnchor constraintEqualToConstant:_avatarSize]
    ]];

    [containerView addSubview:_nonAvatarContainer];
    AddSameCenterConstraints(_nonAvatarContainer, containerView);

    UIImage* peopleWaitingImage = DefaultSymbolWithPointSize(
        kPersonClockFillSymbol, _avatarSize * kPersonWaitingProportion);
    UIImageView* peopleWaitingImageView =
        [[UIImageView alloc] initWithImage:peopleWaitingImage];
    peopleWaitingImageView.contentMode = UIViewContentModeCenter;
    peopleWaitingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    peopleWaitingImageView.tintColor = UIColor.whiteColor;

    [_nonAvatarContainer addSubview:peopleWaitingImageView];
    [NSLayoutConstraint activateConstraints:@[
      [peopleWaitingImageView.centerXAnchor
          constraintEqualToAnchor:_nonAvatarContainer.centerXAnchor],
      [peopleWaitingImageView.centerYAnchor
          constraintEqualToAnchor:_nonAvatarContainer.centerYAnchor
                         constant:-kPersonWaitingVerticalMargin],
    ]];

    [self updateColors];

    return;
  }

  // Display the `plusXLabel` if needed.
  NSInteger remainingCount = totalNumber - _avatars.count;
  if (remainingCount > 0) {
    UIView* plusXLabel =
        [self createPlusXLabelWithRemainingCount:remainingCount];

    // Create a container view to act as the border for the plusXLabel.
    // This is needed to avoid anti-aliasing artifacts around the border itself.
    UIView* plusXContainerView =
        [self createCircularContainerWithSize:containerSize];
    [UIView performWithoutAnimation:^{
      plusXContainerView.layer.cornerRadius = containerSize / 2.0;
    }];
    [plusXContainerView addSubview:plusXLabel];

    [_facesStackView addArrangedSubview:plusXContainerView];

    [NSLayoutConstraint activateConstraints:@[
      [plusXLabel.widthAnchor
          constraintGreaterThanOrEqualToConstant:_avatarSize],
      [plusXLabel.heightAnchor constraintEqualToConstant:_avatarSize],
      [plusXLabel.leadingAnchor
          constraintEqualToAnchor:plusXContainerView.leadingAnchor
                         constant:kContainerSubstractredStroke],
      [plusXLabel.trailingAnchor
          constraintEqualToAnchor:plusXContainerView.trailingAnchor
                         constant:-kContainerSubstractredStroke],

      [plusXContainerView.heightAnchor constraintEqualToConstant:containerSize]
    ]];
    AddSameCenterConstraints(plusXLabel, plusXContainerView);
  }
}

- (void)setAvatarSize:(CGFloat)avatarSize {
  _avatarSize = avatarSize;
  CGFloat containerSize = _avatarSize + kContainerSubstractredStroke * 2;
  self.layer.cornerRadius = containerSize / 2.0;
}

#pragma mark - Private

// Updates colors after UITrait collection update.
- (void)updateColors {
  BOOL isDarkMode =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  if (_facePileBackgroundColor) {
    _nonAvatarContainer.backgroundColor =
        [isDarkMode ? [UIColor colorNamed:kSolidWhiteColor]
                    : [UIColor colorNamed:kSolidBlackColor]
            colorWithAlphaComponent:kNonAvatarContainerBackgroundAlpha];
    _plusXLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
    return;
  }
  if (isDarkMode) {
    _nonAvatarContainer.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    _plusXLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  } else {
    _nonAvatarContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];
    _plusXLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
}

// Creates and configures a circular UIView with a given size.
// This is used for both avatar containers and the "+X" label container.
- (UIView*)createCircularContainerWithSize:(CGFloat)size {
  UIView* container =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, size, size)];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  container.backgroundColor = _facePileBackgroundColor;
  container.layer.masksToBounds = YES;
  return container;
}

// Applies a custom, "cut-off" circular mask to a `container`, creating an
// overlapping visual effect.
- (void)applyOverlappingCircleMaskToContainer:(UIView*)container {
  CGFloat diameter = container.frame.size.width;
  CGFloat radius = diameter / 2.0;
  CGFloat cutOffset = fabs(kAvatarSpacing);
  CGPoint center = CGPointMake(diameter + radius - cutOffset, radius);
  UIBezierPath* offsetPath = [UIBezierPath bezierPathWithArcCenter:center
                                                            radius:radius
                                                        startAngle:0
                                                          endAngle:2 * M_PI
                                                         clockwise:YES];

  UIBezierPath* circlePath =
      [UIBezierPath bezierPathWithRoundedRect:container.frame
                                 cornerRadius:diameter / 2];
  circlePath.usesEvenOddFillRule = YES;
  [circlePath appendPath:offsetPath.bezierPathByReversingPath];

  CAShapeLayer* maskLayer = [CAShapeLayer layer];
  maskLayer.path = circlePath.CGPath;
  container.layer.mask = maskLayer;
}

// Creates and configures the a "+X" label.
- (UIView*)createPlusXLabelWithRemainingCount:(NSInteger)remainingCount {
  // Configure the label.
  UILabel* plusXLabel = [[UILabel alloc] init];
  plusXLabel.text = [NSString stringWithFormat:@"+%ld", (long)remainingCount];
  plusXLabel.translatesAutoresizingMaskIntoConstraints = NO;
  plusXLabel.textAlignment = NSTextAlignmentCenter;
  plusXLabel.font = [UIFont systemFontOfSize:kPlusXlabelFontSize
                                      weight:UIFontWeightMedium];
  [plusXLabel setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [plusXLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Configure a container in order to add an inner horizontal margin around the
  // label.
  UIView* plusXLabelContainer = [[UIView alloc] init];
  plusXLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [UIView performWithoutAnimation:^{
    plusXLabelContainer.layer.cornerRadius = _avatarSize / 2.0;
  }];
  plusXLabelContainer.layer.masksToBounds = YES;
  [plusXLabelContainer addSubview:plusXLabel];

  [NSLayoutConstraint activateConstraints:@[
    [plusXLabel.leadingAnchor
        constraintEqualToAnchor:plusXLabelContainer.leadingAnchor
                       constant:kPlusXlabelContainerHorizontalInnerMargin],
    [plusXLabel.trailingAnchor
        constraintEqualToAnchor:plusXLabelContainer.trailingAnchor
                       constant:-kPlusXlabelContainerHorizontalInnerMargin],
  ]];
  AddSameCenterConstraints(plusXLabelContainer, plusXLabel);

  _plusXLabel = plusXLabel;
  _nonAvatarContainer = plusXLabelContainer;

  [self updateColors];
  return _nonAvatarContainer;
}

// Adds and configures the `_emptyFacePileLabel`.
- (void)addEmptyFacePileLabel {
  _emptyFacePileLabel = [[UILabel alloc] init];
  _emptyFacePileLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_emptyFacePileLabel];
  AddSameConstraints(self, _emptyFacePileLabel);
}

// Returns whether this facepile is empty.
- (BOOL)isEmpty {
  return _facesStackView.arrangedSubviews.count == 0;
}

@end
