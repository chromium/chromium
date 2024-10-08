// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The spacing between the title and description.
constexpr CGFloat kTitleDescriptionSpacing = 2;

// The spacing between elements within the item.
constexpr CGFloat kContentStackSpacing = 14;

// Constants related to the icon container view.
constexpr CGFloat kIconContainerSize = 56;
constexpr CGFloat kIconContainerCornerRadius = 12;

// The size of the checkmark icon.
constexpr CGFloat kCheckmarkSize = 19;
constexpr CGFloat kCheckmarkTopOffset = -6;
constexpr CGFloat kCheckmarkTrailingOffset = 6;

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 22;

// Constants related to Badge Icon sizing.
constexpr CGFloat kBadgeIconSize = 14;

// Constants related to Badge Icon container sizing and positioning.
constexpr CGFloat kBadgeIconContainerSize = 20;
constexpr CGFloat kBadgeIconCircleContainerRadius = kBadgeIconContainerSize / 2;
constexpr CGFloat kBadgeTopOffset = -25;
constexpr CGFloat kBadgeTrailingOffset = -6;

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

// Creates and returns a badge icon `UIImageView` with `_badgeSymbolName` using
// `default_symbol` The icon is configured with a specific size and has auto
// layout constraints activated for its width and height.
UIImageView* BadgeIcon(NSString* badge_symbol_name, BOOL default_symbol) {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  UIImageSymbolConfiguration* color_config = [UIImageSymbolConfiguration
      configurationWithPaletteColors:@[ [UIColor whiteColor] ]];

  config = [config configurationByApplyingConfiguration:color_config];

  UIImage* image =
      default_symbol ? DefaultSymbolWithConfiguration(badge_symbol_name, config)
                     : CustomSymbolWithConfiguration(badge_symbol_name, config);

  UIImageView* badge_icon = [[UIImageView alloc] initWithImage:image];

  badge_icon.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [badge_icon.widthAnchor constraintEqualToConstant:kBadgeIconSize],
    [badge_icon.heightAnchor constraintEqualToAnchor:badge_icon.widthAnchor],
  ]];

  return badge_icon;
}

// Creates and returns a badge icon container `UIView` with `icon` and
// `containerColor` The icon is configured with a specific size and has auto
// layout constraints activated for its width and height.
UIView* BadgeIconInContainer(UIImageView* icon, UIColor* container_color) {
  UIView* circle_view = [[UIView alloc] init];

  circle_view.translatesAutoresizingMaskIntoConstraints = NO;
  circle_view.layer.cornerRadius = kBadgeIconCircleContainerRadius;
  circle_view.backgroundColor = container_color;

  icon.contentMode = UIViewContentModeScaleAspectFit;

  [circle_view addSubview:icon];

  AddSameCenterConstraints(icon, circle_view);

  AddSquareConstraints(circle_view, kBadgeIconContainerSize);

  return circle_view;
}

}  // namespace

@implementation IconDetailView {
  // The title to be displayed in the view.
  NSString* _title;

  // The descriptive text to be displayed in the view.
  NSString* _description;

  // The item layout type. This determines the spacing of elements within the
  // view.
  IconDetailViewLayoutType _layoutType;

  // The symbol to be displayed in the view.
  NSString* _symbolName;

  // The color palette of the symbol displayed in the view.
  NSArray<UIColor*>* _symbolColorPalette;

  // The background color of the symbol displayed in the view.
  UIColor* _symbolBackgroundColor;

  // The width of the symbol.
  CGFloat _symbolWidth;

  // Indicates whether the symbol is a default symbol.
  BOOL _usesDefaultSymbol;

  // Whether or not the icon should be displayed with a green checkmark to
  // indicate a completed state.
  BOOL _showCheckmark;

  // The symbol name of the Badge Icon to be displayed in the view.
  NSString* _badgeSymbolName;

  // The background color of the Badge Icon to be displayed in the view.
  UIColor* _badgeBackgroundColor;

  // Indicates whether the Badge's Icon is a default symbol.
  BOOL _badgeUsesDefaultSymbol;

  // UI tap gesture recognizer. This recognizer detects taps on the view
  // and triggers the appropriate action.
  UITapGestureRecognizer* _tapGestureRecognizer;

  // The accessibility identifier for the view
  NSString* _accessibilityIdentifier;
}

- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                  symbolWidth:(CGFloat)symbolWidth
                showCheckmark:(BOOL)showCheckmark
              badgeSymbolName:(NSString*)badgeSymbolName
         badgeBackgroundColor:(UIColor*)badgeBackgroundColor
       badgeUsesDefaultSymbol:(BOOL)badgeUsesDefaultSymbol
      accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  if ((self = [super init])) {
    _title = title;
    _description = description;
    _layoutType = layoutType;
    _symbolName = symbolName;
    _symbolColorPalette = symbolColorPalette;
    _symbolBackgroundColor = symbolBackgroundColor;
    _usesDefaultSymbol = usesDefaultSymbol;
    _symbolWidth = symbolWidth;
    _showCheckmark = showCheckmark;
    _badgeSymbolName = badgeSymbolName;
    _badgeBackgroundColor = badgeBackgroundColor;
    _badgeUsesDefaultSymbol = badgeUsesDefaultSymbol;
    _accessibilityIdentifier = accessibilityIdentifier;
  }

  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
              badgeSymbolName:(NSString*)badgeSymbolName
         badgeBackgroundColor:(UIColor*)badgeBackgroundColor
       badgeUsesDefaultSymbol:(BOOL)badgeUsesDefaultSymbol
      accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  return [self initWithTitle:title
                  description:description
                   layoutType:layoutType
                   symbolName:symbolName
           symbolColorPalette:symbolColorPalette
        symbolBackgroundColor:symbolBackgroundColor
            usesDefaultSymbol:usesDefaultSymbol
                  symbolWidth:kIconSize
                showCheckmark:showCheckmark
              badgeSymbolName:badgeSymbolName
         badgeBackgroundColor:badgeBackgroundColor
       badgeUsesDefaultSymbol:badgeUsesDefaultSymbol
      accessibilityIdentifier:accessibilityIdentifier];
}

- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  return [self initWithTitle:title
                  description:description
                   layoutType:layoutType
                   symbolName:symbolName
           symbolColorPalette:symbolColorPalette
        symbolBackgroundColor:symbolBackgroundColor
            usesDefaultSymbol:usesDefaultSymbol
                  symbolWidth:kIconSize
                showCheckmark:showCheckmark
              badgeSymbolName:nil
         badgeBackgroundColor:nil
       badgeUsesDefaultSymbol:NO
      accessibilityIdentifier:accessibilityIdentifier];
}

- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                  symbolWidth:(CGFloat)symbolWidth
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  return [self initWithTitle:title
                  description:description
                   layoutType:layoutType
                   symbolName:symbolName
           symbolColorPalette:@[ [UIColor whiteColor] ]
        symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
            usesDefaultSymbol:usesDefaultSymbol
                  symbolWidth:kIconSize
                showCheckmark:showCheckmark
              badgeSymbolName:nil
         badgeBackgroundColor:nil
       badgeUsesDefaultSymbol:NO
      accessibilityIdentifier:accessibilityIdentifier];
}

- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  return [self initWithTitle:title
                  description:description
                   layoutType:layoutType
                   symbolName:symbolName
           symbolColorPalette:@[ [UIColor whiteColor] ]
        symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
            usesDefaultSymbol:usesDefaultSymbol
                  symbolWidth:kIconSize
                showCheckmark:showCheckmark
              badgeSymbolName:nil
         badgeBackgroundColor:nil
       badgeUsesDefaultSymbol:NO
      accessibilityIdentifier:accessibilityIdentifier];
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", _title, _description];
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
  self.accessibilityIdentifier = _accessibilityIdentifier;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  // Add a horizontal stack to contain the icon, text stack, and (optional)
  // chevron.
  NSMutableArray* arrangedSubviews = [[NSMutableArray alloc] init];

  BOOL isHeroLayout = _layoutType == IconDetailViewLayoutType::kHero;

  IconView* icon =
      _usesDefaultSymbol
          ? [[IconView alloc] initWithDefaultSymbol:_symbolName
                                 symbolColorPalette:_symbolColorPalette
                              symbolBackgroundColor:_symbolBackgroundColor
                                        symbolWidth:_symbolWidth
                                      compactLayout:!isHeroLayout
                                           inSquare:YES]
          : [[IconView alloc] initWithCustomSymbol:_symbolName
                                symbolColorPalette:_symbolColorPalette
                             symbolBackgroundColor:_symbolBackgroundColor
                                       symbolWidth:_symbolWidth
                                     compactLayout:!isHeroLayout
                                          inSquare:YES];

  // When the item is displayed in a hero-style layout, the icon is more
  // prominently displayed via an icon container view.
  if (isHeroLayout) {
    UIView* iconContainerView = [self iconInContainer:icon];

    // Display a green checkmark when the layout is hero-cell complete.
    if (_showCheckmark) {
      UIImageView* checkmark = CheckmarkIcon();

      [iconContainerView addSubview:checkmark];

      [NSLayoutConstraint activateConstraints:@[
        [checkmark.topAnchor constraintEqualToAnchor:iconContainerView.topAnchor
                                            constant:kCheckmarkTopOffset],
        [checkmark.trailingAnchor
            constraintEqualToAnchor:iconContainerView.trailingAnchor
                           constant:kCheckmarkTrailingOffset],
      ]];
    }

    // Create the Badge Icon, if applicable.
    if (_badgeSymbolName.length != 0) {
      UIImageView* badge = BadgeIcon(_badgeSymbolName, _badgeUsesDefaultSymbol);

      UIView* badgeWithContainer =
          BadgeIconInContainer(badge, _badgeBackgroundColor);

      [iconContainerView addSubview:badgeWithContainer];

      [NSLayoutConstraint activateConstraints:@[
        [badgeWithContainer.topAnchor
            constraintEqualToAnchor:iconContainerView.bottomAnchor
                           constant:kBadgeTopOffset],
        [badgeWithContainer.trailingAnchor
            constraintEqualToAnchor:iconContainerView.trailingAnchor
                           constant:kBadgeTrailingOffset],
      ]];
    }

    [arrangedSubviews addObject:iconContainerView];
  } else {
    [arrangedSubviews addObject:icon];
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

// Returns `icon` wrapped in a container view. This is used for the hero layout
// to give the icon a more prominent appearance.
- (UIView*)iconInContainer:(IconView*)icon {
  icon.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* iconContainer = [[UIView alloc] init];

  iconContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];
  iconContainer.layer.cornerRadius = kIconContainerCornerRadius;

  [iconContainer addSubview:icon];

  AddSameCenterConstraints(icon, iconContainer);

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [iconContainer.widthAnchor
        constraintEqualToAnchor:iconContainer.heightAnchor],
  ]];

  return iconContainer;
}

// Creates the title label with appropriate font, color, and line break mode.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];

  label.text = _title;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font =
      _layoutType == IconDetailViewLayoutType::kHero
          ? CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold)
          : [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  return label;
}

// Creates the description label with appropriate font, color, and line break
// mode.
- (UILabel*)createDescriptionLabel {
  UILabel* label = [[UILabel alloc] init];

  label.text = _description;
  label.numberOfLines = 2;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

@end
