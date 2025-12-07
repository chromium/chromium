// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_view.h"

#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
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
const CGFloat kPlusXlabelContainerHorizontalInnerMargin = 3;

// Alpha value of the non-avatar view background color.
const CGFloat kNonAvatarContainerBackgroundAlpha = 0.2;

// Proportion of the "person waiting" icon compared to the avatar size.
const CGFloat kPersonWaitingProportion = 0.55;

// Vertical margin for the "person waiting" icon.
const CGFloat kPersonWaitingVerticalMargin = 1.5;

// Share button constants.
const CGFloat kShareElementSpacing = 7;
const CGFloat kShareHorizontalInset = 8;
const CGFloat kShareVerticalInset = 5;
const CGFloat kShareSymbolPointSize = 12.5;

// Returns the background configuration for the buttons.
UIBackgroundConfiguration* BackgroundConfiguration() {
  UIBackgroundConfiguration* background_configuration =
      [UIBackgroundConfiguration clearConfiguration];
  background_configuration.visualEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterial];
  background_configuration.backgroundColor =
      TabGroupViewButtonBackgroundColor();
  return background_configuration;
}

}  // namespace

@implementation FacePileView {
  // Array of avatar primitive objects, each representing a face in the pile.
  NSArray<id<ShareKitAvatarPrimitive>>* _avatars;
  // A UIStackView to arrange and display the individual avatar views.
  UIStackView* _facesStackView;
  // Sets the background color for the face pile, visible in gaps and as an
  // outer stroke.
  UIColor* _facePileBackgroundColor;
  // Size of avatar faces, in points.
  CGFloat _avatarSize;
  // Container for the non-avatar element.
  UIView* _nonAvatarContainer;
  // Container for the people waiting image.
  UIImageView* _peopleWaitingImageView;
  // Label to display the "+X" person in the group.
  UILabel* _plusXLabel;
  // The number of members in the shared group.
  CGFloat _membersCount;
  // View to display when the face pile is empty.
  UIButton* _shareViewContainer;
  // The background of the non-avatar.
  UIView* _nonAvatarBackground;
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

#pragma mark - Getters

- (CGFloat)optimalWidth {
  NSInteger containerCount = _membersCount < 3 ? 2 : 3;
  CGFloat containerSize = _avatarSize + kContainerSubstractredStroke * 2;

  CGFloat width = (containerCount - 1) * kAvatarSpacing;
  width += containerCount * containerSize;

  if (_membersCount > 3) {
    // Subtract the width of the last avatar, as it's replaced by the "+X"
    // container.
    width -= _avatarSize;
    width +=
        [_plusXLabel.text sizeWithAttributes:@{
          NSFontAttributeName : [UIFont systemFontOfSize:kPlusXlabelFontSize
                                                  weight:UIFontWeightMedium]
        }]
            .width;
    width += 2 * kPlusXlabelContainerHorizontalInnerMargin;
  }

  return width;
}

#pragma mark - FacePileConsumer

- (void)setSharedButtonWhenEmpty:(BOOL)showsShareButtonWhenEmpty {
  [_shareViewContainer removeFromSuperview];
  _shareViewContainer = nil;
  if (showsShareButtonWhenEmpty && [self isEmpty]) {
    [self addShareButtonView];
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
  _avatars = faces;
  _membersCount = totalNumber;

  __weak __typeof(self) weakSelf = self;
  [UIView performWithoutAnimation:^{
    [weakSelf setupStackView];
  }];
}

- (void)setAvatarSize:(CGFloat)avatarSize {
  _avatarSize = avatarSize;
}

#pragma mark - Private

// Updates colors after UITrait collection update.
- (void)updateColors {
  if (_facePileBackgroundColor) {
    [_nonAvatarBackground removeFromSuperview];
    _nonAvatarContainer.tintColor = [UIColor colorNamed:kSolidWhiteColor];
    _plusXLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
    _nonAvatarContainer.backgroundColor = [[UIColor colorNamed:kSolidBlackColor]
        colorWithAlphaComponent:kNonAvatarContainerBackgroundAlpha];
  } else {
    _peopleWaitingImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _plusXLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
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
  container.layer.cornerRadius = size / 2.0;
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

  _nonAvatarBackground = [self createNonAvatarBackground];

  // Configure a container in order to add an inner horizontal margin around the
  // label.
  UIView* plusXLabelContainer = [[UIView alloc] init];
  plusXLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
  plusXLabelContainer.layer.cornerRadius = _avatarSize / 2.0;
  plusXLabelContainer.layer.masksToBounds = YES;
  [plusXLabelContainer addSubview:_nonAvatarBackground];
  [plusXLabelContainer addSubview:plusXLabel];

  AddSameConstraints(plusXLabelContainer, _nonAvatarBackground);

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

// Creates the background view for the non-avatar.
- (UIView*)createNonAvatarBackground {
  // Use a button to ensure to have the same configuration as the others.
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.background = BackgroundConfiguration();

  UIView* background = [UIButton buttonWithConfiguration:configuration
                                           primaryAction:nil];
  background.userInteractionEnabled = NO;

  background.translatesAutoresizingMaskIntoConstraints = NO;
  return background;
}

// Adds and configures the `_emptyFacePileLabel`.
- (void)addShareButtonView {
  UIImage* shareSymbol = DefaultSymbolWithConfiguration(
      kPersonFillBadgePlusSymbol,
      [UIImageSymbolConfiguration
          configurationWithPointSize:kShareSymbolPointSize
                              weight:UIImageSymbolWeightBold
                               scale:UIImageSymbolScaleMedium]);

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  configuration.baseForegroundColor = UIColor.whiteColor;
  configuration.background = BackgroundConfiguration();
  configuration.image = shareSymbol;
  configuration.imagePadding = kShareElementSpacing;
  configuration.contentInsets =
      NSDirectionalEdgeInsetsMake(kShareVerticalInset, kShareHorizontalInset,
                                  kShareVerticalInset, kShareHorizontalInset);
  configuration.titleLineBreakMode = NSLineBreakByClipping;

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_SHARED_GROUP_SHARE_GROUP_BUTTON_TEXT)];
  [string addAttributes:attributes range:NSMakeRange(0, string.length)];
  configuration.attributedTitle = string;

  // To ensure design consistency with other buttons, create a disabled button
  // here. Its action is handled elsewhere.
  _shareViewContainer = [UIButton buttonWithConfiguration:configuration
                                            primaryAction:nil];
  _shareViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _shareViewContainer.userInteractionEnabled = NO;
  [_shareViewContainer.heightAnchor
      constraintEqualToConstant:kTabGroupButtonHeight]
      .active = YES;

  [self addSubview:_shareViewContainer];
  AddSameConstraints(self, _shareViewContainer);
}

// Returns whether this facepile is empty.
- (BOOL)isEmpty {
  return _facesStackView.arrangedSubviews.count == 0;
}

// Configures and populates the stack view with avatar representations.
// Clears any previously subviews, then adds individual avatars, a "plus X"
// label, or a placeholder as needed.
- (void)setupStackView {
  // Remove all existing avatar views from the stack.
  for (UIView* view in _facesStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  CGFloat containerSize = _avatarSize + kContainerSubstractredStroke * 2;
  NSInteger avatarsCount = static_cast<NSUInteger>(_avatars.count);
  self.layer.cornerRadius = containerSize / 2.0;

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
    if (index + 1 < _membersCount || _membersCount == 1) {
      [self applyOverlappingCircleMaskToContainer:avatarContainerView];
    }

    [NSLayoutConstraint activateConstraints:@[
      [avatarContainerView.widthAnchor constraintEqualToConstant:containerSize],
      [avatarContainerView.heightAnchor constraintEqualToConstant:containerSize]
    ]];
    AddSameCenterConstraints(avatarView, avatarContainerView);

    [_facesStackView addArrangedSubview:avatarContainerView];
  }

  // Update the `_shareViewContainer` visibility.
  _shareViewContainer.hidden = ![self isEmpty];

  if (_membersCount == 1) {
    // Create a container view to act as the border.
    // This is needed to avoid anti-aliasing artifacts around the border itself.
    UIView* containerView =
        [self createCircularContainerWithSize:containerSize];
    [_facesStackView addArrangedSubview:containerView];

    [NSLayoutConstraint activateConstraints:@[
      [containerView.widthAnchor constraintEqualToConstant:containerSize],
      [containerView.heightAnchor constraintEqualToConstant:containerSize]
    ]];

    _nonAvatarBackground = [self createNonAvatarBackground];

    _nonAvatarContainer = [[UIView alloc] init];
    _nonAvatarContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _nonAvatarContainer.layer.cornerRadius = _avatarSize / 2.0;
    _nonAvatarContainer.layer.masksToBounds = YES;
    [NSLayoutConstraint activateConstraints:@[
      [_nonAvatarContainer.widthAnchor constraintEqualToConstant:_avatarSize],
      [_nonAvatarContainer.heightAnchor constraintEqualToConstant:_avatarSize]
    ]];

    [_nonAvatarContainer addSubview:_nonAvatarBackground];
    AddSameConstraints(_nonAvatarContainer, _nonAvatarBackground);

    [containerView addSubview:_nonAvatarContainer];
    AddSameCenterConstraints(_nonAvatarContainer, containerView);

    UIImage* peopleWaitingImage = DefaultSymbolWithPointSize(
        kPersonClockFillSymbol, _avatarSize * kPersonWaitingProportion);
    _peopleWaitingImageView =
        [[UIImageView alloc] initWithImage:peopleWaitingImage];
    _peopleWaitingImageView.contentMode = UIViewContentModeCenter;
    _peopleWaitingImageView.translatesAutoresizingMaskIntoConstraints = NO;

    [_nonAvatarContainer addSubview:_peopleWaitingImageView];
    [NSLayoutConstraint activateConstraints:@[
      [_peopleWaitingImageView.centerXAnchor
          constraintEqualToAnchor:_nonAvatarContainer.centerXAnchor],
      [_peopleWaitingImageView.centerYAnchor
          constraintEqualToAnchor:_nonAvatarContainer.centerYAnchor
                         constant:-kPersonWaitingVerticalMargin],
    ]];

    [self updateColors];

    return;
  }

  // Display the `plusXLabel` if needed.
  NSInteger remainingCount = _membersCount - _avatars.count;
  if (remainingCount > 0) {
    UIView* plusXLabel =
        [self createPlusXLabelWithRemainingCount:remainingCount];

    // Create a container view to act as the border for the plusXLabel.
    // This is needed to avoid anti-aliasing artifacts around the border itself.
    UIView* plusXContainerView =
        [self createCircularContainerWithSize:containerSize];
    plusXContainerView.layer.cornerRadius = containerSize / 2.0;
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

@end
