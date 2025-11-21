// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_detail_view.h"

#import <optional>

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_detail_view_configuration.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view_configuration.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The spacing between the title and description.
constexpr CGFloat kTitleDescriptionSpacing = 2;

// The spacing between elements within the item.
constexpr CGFloat kContentStackSpacing = 14;

// Constants related to the icon container view.
constexpr CGFloat kIconContainerSize = 56;
constexpr CGFloat kIconContainerSizeLarge = 72;
constexpr CGFloat kIconContainerCornerRadius = 12;

// The size of the checkmark icon.
constexpr CGFloat kCheckmarkSize = 19;
constexpr CGFloat kCheckmarkTopOffset = -6;
constexpr CGFloat kCheckmarkTrailingOffset = 6;

// Constants related to background image.

// Alpha for top of gradient overlay.
const CGFloat kGradientOverlayTopAlpha = 0.0;

// Alpha for bottom of gradient overlay.
const CGFloat kGradientOverlayBottomAlpha = 0.14;

// Width and height of the background image.
const CGFloat kBackgroundImageWidthHeight = 72.0;

// Rounded corners of the background image.
const CGFloat kBackgroundImageCornerRadius = 12.0;

// Creates and returns a checkmark icon `UIImageView` with a green checkmark
// symbol. The icon is configured with a specific size and has auto layout
// constraints activated for its width and height.
UIImageView* CheckmarkIcon() {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  UIImageSymbolConfiguration* colorConfig =
      [UIImageSymbolConfiguration configurationWithPaletteColors:@[
        [UIColor whiteColor], [UIColor colorNamed:kGreen500Color]
      ]];

  config = [config configurationByApplyingConfiguration:colorConfig];

  UIImage* image =
      DefaultSymbolWithConfiguration(kCheckmarkCircleFillSymbol, config);

  UIImageView* icon = [[UIImageView alloc] initWithImage:image];

  icon.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor constraintEqualToConstant:kCheckmarkSize],
    [icon.heightAnchor constraintEqualToAnchor:icon.widthAnchor],
  ]];

  return icon;
}

// Creates and returns a badge icon `UIImageView` with `badge_symbol_name` using
// `default_symbol`. The icon is configured with a specific size relative to its
// container and has auto layout constraints activated for its width and height.
//
// The `color_palette` argument allows you to specify an array of colors to use
// for the badge icon. If `color_palette` is `nil` or empty, the default system
// colors will be used.
//
// The `has_background_image` argument indicates whether the icon is displayed
// with a background image. This information is used to adjust the size of the
// badge icon.
UIImageView* BadgeIcon(
    const IconDetailViewConfiguration* icon_detail_view_configuration) {
  CHECK(icon_detail_view_configuration);
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  UIImageSymbolConfiguration* color_config = [UIImageSymbolConfiguration
      configurationWithPaletteColors:icon_detail_view_configuration
                                         .badgeColorPalette];

  config = [config configurationByApplyingConfiguration:color_config];

  UIImage* image =
      icon_detail_view_configuration.badgeUsesDefaultSymbol
          ? DefaultSymbolWithConfiguration(
                icon_detail_view_configuration.badgeSymbolName, config)
          : CustomSymbolWithConfiguration(
                icon_detail_view_configuration.badgeSymbolName, config);

  if (!icon_detail_view_configuration.badgeColorPalette) {
    image = MakeSymbolMulticolor(image);
  }

  UIImageView* badge_icon = [[UIImageView alloc] initWithImage:image];

  badge_icon.translatesAutoresizingMaskIntoConstraints = NO;

  // Calculate badge size based on container size and background image presence.
  CGFloat badge_size =
      icon_detail_view_configuration.badgeShapeConfig.size /
      (icon_detail_view_configuration.backgroundImage ? 1.15 : 1.5);

  [NSLayoutConstraint activateConstraints:@[
    [badge_icon.widthAnchor constraintEqualToConstant:badge_size],
    [badge_icon.heightAnchor constraintEqualToAnchor:badge_icon.widthAnchor],
  ]];

  return badge_icon;
}

// Helper function to create a rounded corner path.
void AddRoundedCornerToPath(UIBezierPath* bezier_path,
                            CGFloat radius,
                            CGPoint center,
                            CGFloat start_angle,
                            CGFloat end_angle) {
  [bezier_path addArcWithCenter:center
                         radius:radius
                     startAngle:start_angle
                       endAngle:end_angle
                      clockwise:YES];
}

UIView* BadgeIconInContainer(UIImageView* icon,
                             UIColor* container_color,
                             BadgeShapeConfig badge_shape_config) {
  UIView* container = [[UIView alloc] init];

  container.translatesAutoresizingMaskIntoConstraints = NO;

  // Create the Bezier path based on the `badge_shape_config`.
  UIBezierPath* bezier_path = [UIBezierPath bezierPath];

  switch (badge_shape_config.shape) {
    case IconDetailViewBadgeShape::kCircle: {
      // If it's a circle, adjust the path to a circular shape.
      CGFloat radius = badge_shape_config.size / 2.0;
      bezier_path = [UIBezierPath
          bezierPathWithRoundedRect:CGRectMake(0, 0, badge_shape_config.size,
                                               badge_shape_config.size)
                       cornerRadius:radius];
      break;
    }
    case IconDetailViewBadgeShape::kSquare: {
      CGFloat size = badge_shape_config.size;

      // Start at the top-left corner
      [bezier_path
          moveToPoint:CGPointMake(badge_shape_config.topLeftRadius, 0)];

      // Top edge
      [bezier_path
          addLineToPoint:CGPointMake(size - badge_shape_config.topRightRadius,
                                     0)];
      AddRoundedCornerToPath(
          bezier_path, badge_shape_config.topRightRadius,
          CGPointMake(size - badge_shape_config.topRightRadius,
                      badge_shape_config.topRightRadius),
          M_PI * 1.5, 0);

      // Right edge
      [bezier_path
          addLineToPoint:CGPointMake(
                             size,
                             size - badge_shape_config.bottomRightRadius)];
      AddRoundedCornerToPath(
          bezier_path, badge_shape_config.bottomRightRadius,
          CGPointMake(size - badge_shape_config.bottomRightRadius,
                      size - badge_shape_config.bottomRightRadius),
          0, M_PI_2);

      // Bottom edge
      [bezier_path
          addLineToPoint:CGPointMake(badge_shape_config.bottomLeftRadius,
                                     size)];
      AddRoundedCornerToPath(
          bezier_path, badge_shape_config.bottomLeftRadius,
          CGPointMake(badge_shape_config.bottomLeftRadius,
                      size - badge_shape_config.bottomLeftRadius),
          M_PI_2, M_PI);

      // Left edge
      [bezier_path
          addLineToPoint:CGPointMake(0, badge_shape_config.topLeftRadius)];
      AddRoundedCornerToPath(bezier_path, badge_shape_config.topLeftRadius,
                             CGPointMake(badge_shape_config.topLeftRadius,
                                         badge_shape_config.topLeftRadius),
                             M_PI, M_PI * 1.5);

      [bezier_path closePath];

      break;
    }
  }

  // Create a shape layer with the Bezier path.
  CAShapeLayer* mask = [CAShapeLayer layer];
  mask.path = bezier_path.CGPath;
  container.layer.mask = mask;

  container.backgroundColor = container_color;

  icon.contentMode = UIViewContentModeScaleAspectFit;

  [container addSubview:icon];

  AddSameCenterConstraints(icon, container);

  AddSquareConstraints(container, badge_shape_config.size);

  return container;
}

}  // namespace

@implementation IconDetailView {
  // Configuration for this view.
  IconDetailViewConfiguration* _configuration;
  // UI tap gesture recognizer. This recognizer detects taps on the view
  // and triggers the appropriate action.
  UITapGestureRecognizer* _tapGestureRecognizer;
}

- (instancetype)initWithConfiguration:
    (IconDetailViewConfiguration*)configuration {
  if ((self = [super initWithFrame:CGRectZero])) {
    CHECK(configuration);
    _configuration = [configuration copy];
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", _configuration.titleText,
                                    _configuration.descriptionText];
}

#pragma mark - Private

// Creates and configures the subviews for the `IconDetailView`. This method
// sets up the icon, title, description, and optional chevron, arranging them
// based on the layout type. It also handles accessibility and adds a tap
// gesture recognizer.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = _configuration.accessibilityIdentifier;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  // Add a horizontal stack to contain the icon, text stack, and (optional)
  // chevron.
  NSMutableArray* arrangedSubviews = [[NSMutableArray alloc] init];

  IconView* icon = [[IconView alloc]
      initWithConfiguration:[_configuration iconViewConfiguration:YES]];
  UIView* image = icon;

  if (_configuration.backgroundImage) {
    UIImageView* backgroundImageView = [[UIImageView alloc] init];

    backgroundImageView.image = _configuration.backgroundImage;
    backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
    backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundImageView.layer.borderWidth = 0;
    backgroundImageView.layer.cornerRadius = kBackgroundImageCornerRadius;
    backgroundImageView.layer.masksToBounds = YES;
    backgroundImageView.backgroundColor = UIColor.whiteColor;

    UIView* gradientOverlay = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor]
                             colorWithAlphaComponent:kGradientOverlayTopAlpha]
             bottomColor:
                 [[UIColor blackColor]
                     colorWithAlphaComponent:kGradientOverlayBottomAlpha]];

    gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    gradientOverlay.layer.cornerRadius = kBackgroundImageCornerRadius;
    gradientOverlay.layer.zPosition = 1;

    [NSLayoutConstraint activateConstraints:@[
      [image.heightAnchor
          constraintEqualToConstant:kBackgroundImageWidthHeight],
      [image.widthAnchor constraintEqualToAnchor:image.heightAnchor],
      [backgroundImageView.heightAnchor
          constraintEqualToConstant:kBackgroundImageWidthHeight],
      [backgroundImageView.widthAnchor
          constraintEqualToAnchor:backgroundImageView.heightAnchor],
      [gradientOverlay.heightAnchor
          constraintEqualToConstant:kBackgroundImageWidthHeight],
      [gradientOverlay.widthAnchor
          constraintEqualToAnchor:gradientOverlay.heightAnchor],
    ]];

    [image addSubview:backgroundImageView];
    [backgroundImageView addSubview:gradientOverlay];
  }

  image.translatesAutoresizingMaskIntoConstraints = NO;

  BOOL isHeroLayout =
      _configuration.layoutType == IconDetailViewLayoutType::kHero;

  // When the item is displayed in a hero-style layout, the icon is more
  // prominently displayed via an icon container view.
  if (isHeroLayout) {
    UIView* imageContainerView =
        [self imageInContainer:image
             useLargeContainer:(_configuration.backgroundImage != nil)];

    // Display a green checkmark when the layout is hero-cell complete.
    if (_configuration.showCheckmark) {
      UIImageView* checkmark = CheckmarkIcon();

      [imageContainerView addSubview:checkmark];

      [NSLayoutConstraint activateConstraints:@[
        [checkmark.topAnchor
            constraintEqualToAnchor:imageContainerView.topAnchor
                           constant:kCheckmarkTopOffset],
        [checkmark.trailingAnchor
            constraintEqualToAnchor:imageContainerView.trailingAnchor
                           constant:kCheckmarkTrailingOffset],
      ]];
    }

    // Create the Badge Icon, if applicable.
    if (_configuration.badgeSymbolName.length != 0) {
      UIImageView* badge = BadgeIcon(_configuration);

      UIView* badgeWithContainer =
          BadgeIconInContainer(badge, _configuration.badgeBackgroundColor,
                               _configuration.badgeShapeConfig);

      [imageContainerView addSubview:badgeWithContainer];

      // Calculate the offset to equally space the badge from the right and
      // bottom.
      CGFloat badgeOffset = _configuration.badgeShapeConfig.size /
                            (_configuration.backgroundImage ? 5.0 : 3.0);

      [NSLayoutConstraint activateConstraints:@[
        [badgeWithContainer.bottomAnchor
            constraintEqualToAnchor:imageContainerView.bottomAnchor
                           constant:-badgeOffset],
        [badgeWithContainer.trailingAnchor
            constraintEqualToAnchor:imageContainerView.trailingAnchor
                           constant:-badgeOffset],
      ]];
    }

    [arrangedSubviews addObject:imageContainerView];
  } else {
    [arrangedSubviews addObject:image];
  }

  UILabel* titleLabel = [self createTitleLabel];

  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  UILabel* descriptionLabel = [self createDescriptionLabel];

  [descriptionLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];

  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = kTitleDescriptionSpacing;
  [textStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                               forAxis:UILayoutConstraintAxisHorizontal];

  [arrangedSubviews addObject:textStack];

  // For compact layout, display a chevron at the end of the item.
  if (!isHeroLayout) {
    UIImageView* chevron = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];

    [chevron setContentHuggingPriority:UILayoutPriorityDefaultHigh
                               forAxis:UILayoutConstraintAxisHorizontal];

    [arrangedSubviews addObject:chevron];
  }

  UIStackView* contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];

  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.axis = UILayoutConstraintAxisHorizontal;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.spacing = kContentStackSpacing;

  [self addSubview:contentStack];

  AddSameConstraints(contentStack, self);

  // Set up the tap gesture recognizer.
  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];

  [self addGestureRecognizer:_tapGestureRecognizer];
}

// Called when the view is tapped. This method notifies the `tapDelegate`
// that the `IconDetailView` has been tapped.
- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.tapDelegate didTapIconDetailView:self];
  }
}

// Returns `icon` wrapped in a container view with a size determined by
// `useLargeContainer`. This is used for the hero layout to give the icon a
// more prominent appearance. If `useLargeContainer` is YES, the container
// will use `kIconContainerSizeLarge`; otherwise, it will use
// `kIconContainerSizeSmall`.
- (UIView*)imageInContainer:(UIView*)icon
          useLargeContainer:(BOOL)useLargeContainer {
  UIView* iconContainer = [[UIView alloc] init];

  iconContainer.backgroundColor = _configuration.symbolContainerBackgroundColor;
  iconContainer.layer.cornerRadius = kIconContainerCornerRadius;

  [iconContainer addSubview:icon];

  AddSameCenterConstraints(icon, iconContainer);

  CGFloat width =
      useLargeContainer ? kIconContainerSizeLarge : kIconContainerSize;

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor constraintEqualToConstant:width],
    [iconContainer.widthAnchor
        constraintEqualToAnchor:iconContainer.heightAnchor],
  ]];

  return iconContainer;
}

// Creates the title label with appropriate font, color, and line break mode.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];

  label.text = _configuration.titleText;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  if (_configuration.layoutType == IconDetailViewLayoutType::kHero) {
    label.font =
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold,
                                  kMaxTextSizeForStyleFootnote);
  } else {
    label.font = PreferredFontForTextStyle(
        UIFontTextStyleFootnote, std::nullopt, kMaxTextSizeForStyleFootnote);
  }

  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  return label;
}

// Creates the description label with appropriate font, color, and line break
// mode.
- (UILabel*)createDescriptionLabel {
  UILabel* label = [[UILabel alloc] init];

  label.text = _configuration.descriptionText;
  label.numberOfLines = 2;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = PreferredFontForTextStyle(UIFontTextStyleFootnote, std::nullopt,
                                         kMaxTextSizeForStyleFootnote);
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

@end
