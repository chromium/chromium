// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_cell.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_mutator.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"

namespace {
/// The reuse identifier for the input item cells in the carousel.
NSString* const kItemCellReuseIdentifier = @"AIMInputItemCell";
/// The identifier for the main section of the collection view.
NSString* const kMainSectionIdentifier = @"MainSection";

/// The corner radius for the input plate container.
const CGFloat kInputPlateCornerRadius = 24.0f;
/// The shadow opacity for the input plate container.
const float kInputPlateShadowOpacity = 0.2f;
/// The shadow radius for the input plate container.
const CGFloat kInputPlateShadowRadius = 20.0f;
/// The spacing between items in the carousel.
const CGFloat kCarouselItemSpacing = 12.0f;
/// The height of the carousel view.
const CGFloat kCarouselHeight = 42.0f;
/// The height of the AIM mode button.
const CGFloat kAIMButtonHeight = 32.0f;
/// The width of the AIM mode button.
const CGFloat kAIMButtonWidth = 94.0f;
/// The spacing for the horizontal buttons stack view.
const CGFloat kButtonsStackViewSpacing = 18.0f;
/// The spacing for the main vertical input plate stack view.
const CGFloat kInputPlateStackViewSpacing = 16.0f;
/// The padding for the close button.
const CGFloat kCloseButtonPadding = 16.0f;
/// The horizontal and bottom padding for the input plate container.
const CGFloat kInputPlatePadding = 10.0f;
/// The vertical padding for the input plate stack view.
const CGFloat kInputPlateStackViewVerticalPadding = 10.0f;
/// The leading padding for the input plate stack view.
const CGFloat kInputPlateStackViewLeadingPadding = 10.0f;
/// The trailing padding for the input plate stack view.
const CGFloat kInputPlateStackViewTrailingPadding = 12.0f;
/// The font size for the AIM mode button title.
const CGFloat kAIMButtonFontSize = 14.0f;
/// The point size for the symbols in the AIM mode button.
const CGFloat kAIMButtonSymbolPointSize = 12.0f;
/// The width of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonWidth = 24.0f;
/// The height of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonHeight = 32.0f;

/// The duration for the glow effect.
const CGFloat kGlowEffectDuration = 1.0f;
/// The width of the glow effect border.
const CGFloat kGlowEffectWidth = 4.0f;

/// The top padding between the omnibox container and the mic button.
const CGFloat kMicButtonTopPadding = 2.0f;
/// The trailing padding between the omnibox container and the mic button.
const CGFloat kMicButtonTrailingPadding = 5.0f;

/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The alpha for the close button.
const CGFloat kCloseButtonAlpha = 0.6f;
/// The fade view width.
const CGFloat kFadeViewWidth = 30.0f;
}

@interface AIMPrototypeViewController () <UITextViewDelegate,
                                          AIMInputItemCellDelegate>

/// Whether the AI mode is enabled.
@property(nonatomic, assign) BOOL AIModeEnabled;

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

@end

@implementation AIMPrototypeViewController {
  /// The collection view to display the selected images.
  UICollectionView* _carouselView;
  /// The diffable data source for the carousel view.
  UICollectionViewDiffableDataSource<NSString*, AIMInputItem*>* _dataSource;
  /// The view containing the input text field and action buttons.
  UIView* _inputPlateContainerView;
  /// The stack view for the input plate.
  UIStackView* _inputPlateStackView;

  /// The button to toggle AI mode.
  UIButton* _aimButton;
  /// The glow effect around the input plate container.
  UIView<GlowEffect>* _glowEffectView;
  /// The mic button for voice search.
  UIButton* _micButton;
  /// The fade view for the carousel.
  UIView* _carouselFadeView;
  /// The carousel container.
  UIView* _carouselContainer;
  /// Container for the omnibox.
  UIView* _omniboxContainer;
  /// Container for the omnibox popup.
  UIView* _omniboxPopupContainer;
}

/// AIMPrototypeAnimationContextProvider
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;

- (instancetype)init {
  self = [super init];
  if (self) {
    _omniboxContainer = [[UIView alloc] init];
    _omniboxPopupContainer = [[UIView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Close button
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  UIImage* buttonImage =
      SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                       symbolConfiguration),
                        @[
                          [[UIColor tertiaryLabelColor]
                              colorWithAlphaComponent:kCloseButtonAlpha],
                          [UIColor tertiarySystemFillColor]
                        ]);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:closeButton];

  // Omnibox popup container.
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_omniboxPopupContainer];

  [NSLayoutConstraint activateConstraints:@[
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:closeButton.bottomAnchor],
    [_omniboxPopupContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_omniboxPopupContainer.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_omniboxPopupContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // --- Bottom Input Area ---

  // Input plate container
  _inputPlateContainerView = [[UIView alloc] init];
  _inputPlateContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlateContainerView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  _inputPlateContainerView.layer.cornerRadius = kInputPlateCornerRadius;
  _inputPlateContainerView.layer.shadowColor =
      [UIColor colorNamed:kTextPrimaryColor].CGColor;
  _inputPlateContainerView.layer.shadowOpacity = kInputPlateShadowOpacity;
  _inputPlateContainerView.layer.shadowRadius = kInputPlateShadowRadius;
  _inputPlateContainerView.layer.shadowOffset = CGSizeZero;
  [self.view addSubview:_inputPlateContainerView];

  _glowEffectView = ios::provider::CreateGlowEffect(
      CGRectZero, kInputPlateCornerRadius, kGlowEffectWidth);
  if (_glowEffectView) {
    _glowEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    _glowEffectView.userInteractionEnabled = NO;
    [self.view insertSubview:_glowEffectView
                belowSubview:_inputPlateContainerView];
    AddSameConstraints(_inputPlateContainerView, _glowEffectView);
  }

  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _micButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                       kSymbolActionPointSize)];
  [_micButton addTarget:self
                 action:@selector(micButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [_omniboxContainer addSubview:_micButton];
  [NSLayoutConstraint activateConstraints:@[
    [_micButton.topAnchor constraintEqualToAnchor:_omniboxContainer.topAnchor
                                         constant:kMicButtonTopPadding],
    [_micButton.widthAnchor constraintEqualToConstant:kGenericButtonWidth],
    [_micButton.heightAnchor constraintEqualToConstant:kGenericButtonHeight],
    [_micButton.trailingAnchor
        constraintEqualToAnchor:_omniboxContainer.trailingAnchor
                       constant:-kMicButtonTrailingPadding],
  ]];

  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.estimatedItemSize = UICollectionViewFlowLayoutAutomaticSize;
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[AIMInputItemCell class]
      forCellWithReuseIdentifier:kItemCellReuseIdentifier];
  _dataSource = [self createDataSource];
  _carouselView.dataSource = _dataSource;
  [_carouselView.heightAnchor constraintEqualToConstant:kCarouselHeight]
      .active = YES;
  _carouselView.showsHorizontalScrollIndicator = NO;

  // Carousel container and fade view.
  _carouselContainer = [[UIView alloc] init];
  _carouselContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselContainer.hidden = YES;
  [_carouselContainer addSubview:_carouselView];
  AddSameConstraints(_carouselContainer, _carouselView);

  _carouselFadeView = [[UIView alloc] init];
  _carouselFadeView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselFadeView.userInteractionEnabled = NO;
  _carouselFadeView.hidden = YES;
  [_carouselContainer addSubview:_carouselFadeView];

  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.colors = @[
    (id)[[UIColor colorNamed:kPrimaryBackgroundColor]
        colorWithAlphaComponent:0.0]
        .CGColor,
    (id)[UIColor colorNamed:kPrimaryBackgroundColor].CGColor
  ];
  gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  [_carouselFadeView.layer insertSublayer:gradientLayer atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_carouselFadeView.trailingAnchor
        constraintEqualToAnchor:_carouselContainer.trailingAnchor],
    [_carouselFadeView.topAnchor
        constraintEqualToAnchor:_carouselContainer.topAnchor],
    [_carouselFadeView.bottomAnchor
        constraintEqualToAnchor:_carouselContainer.bottomAnchor],
    [_carouselFadeView.widthAnchor constraintEqualToConstant:kFadeViewWidth]
  ]];

  // Action buttons
  UIButton* plusButton =
      [self createButtonWithImage:DefaultSymbolWithPointSize(
                                      kPlusSymbol, kSymbolActionPointSize)];
  [plusButton addTarget:self
                 action:@selector(plusButtonTouchDown)
       forControlEvents:UIControlEventTouchDown];
  plusButton.showsMenuAsPrimaryAction = YES;

  __weak __typeof__(self) weakSelf = self;
  UIAction* galleryAction = [UIAction
      // TODO(crbug.com/40280872): Localize this string.

      actionWithTitle:@"Gallery"
                image:DefaultSymbolWithPointSize(kPhotoSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    aimPrototypeViewControllerDidTapGalleryButton:weakSelf];
              }];
  UIAction* cameraAction = [UIAction
      // TODO(crbug.com/40280872): Localize this string.

      actionWithTitle:@"Camera"
                image:DefaultSymbolWithPointSize(@"camera",
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    aimPrototypeViewControllerDidTapCameraButton:weakSelf];
              }];

  UIAction* fileAction = [UIAction
      // TODO(crbug.com/40280872): Localize this string.
      actionWithTitle:@"File"
                image:DefaultSymbolWithPointSize(@"doc", kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    aimPrototypeViewControllerDidTapFileButton:weakSelf];
              }];

  UIAction* attachCurrentTabAction = [UIAction
      // TODO(crbug.com/40280872): Localize this string.
      actionWithTitle:@"Attach current tab"
                image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.mutator attachCurrentTabContent];
              }];

  plusButton.menu = [UIMenu
      menuWithTitle:@""
           children:@[
             fileAction, galleryAction, cameraAction, attachCurrentTabAction
           ]];

  _aimButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _aimButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_aimButton addTarget:self
                 action:@selector(aimButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [self updateAIMButtonAppearance];

  [_aimButton.heightAnchor constraintEqualToConstant:kAIMButtonHeight].active =
      YES;
  [_aimButton.widthAnchor constraintEqualToConstant:kAIMButtonWidth].active =
      YES;

  // Horizontal stack view for buttons
  UIStackView* buttonsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ plusButton, _aimButton, [UIView new] ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = kButtonsStackViewSpacing;
  buttonsStackView.alignment = UIStackViewAlignmentBottom;

  // Main vertical stack view
  _inputPlateStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _omniboxContainer, _carouselContainer, buttonsStackView
  ]];
  _inputPlateStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlateStackView.axis = UILayoutConstraintAxisVertical;
  _inputPlateStackView.spacing = kInputPlateStackViewSpacing;
  [_inputPlateContainerView addSubview:_inputPlateStackView];

  // Layout.
  [NSLayoutConstraint activateConstraints:@[
    // Close button.
    [closeButton.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kCloseButtonPadding],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kCloseButtonPadding],
    [closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [closeButton.widthAnchor constraintEqualToAnchor:closeButton.heightAnchor],

    // Input Plate.
    [_inputPlateContainerView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kInputPlatePadding],
    [_inputPlateContainerView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kInputPlatePadding],
    [_inputPlateContainerView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kInputPlatePadding],

    // Main Stack View in Plate.
    [_inputPlateStackView.topAnchor
        constraintEqualToAnchor:_inputPlateContainerView.topAnchor
                       constant:kInputPlateStackViewVerticalPadding],
    [_inputPlateStackView.bottomAnchor
        constraintEqualToAnchor:_inputPlateContainerView.bottomAnchor
                       constant:-kInputPlateStackViewVerticalPadding],
    [_inputPlateStackView.leadingAnchor
        constraintEqualToAnchor:_inputPlateContainerView.leadingAnchor
                       constant:kInputPlateStackViewLeadingPadding],
    [_inputPlateStackView.trailingAnchor
        constraintEqualToAnchor:_inputPlateContainerView.trailingAnchor
                       constant:-kInputPlateStackViewTrailingPadding],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update the gradient layer's frame.
  _carouselFadeView.layer.sublayers.firstObject.frame =
      _carouselFadeView.bounds;
  [self updateCarouselFade];
}

#pragma mark - Public

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  _editView = editView;
  _editView.translatesAutoresizingMaskIntoConstraints = NO;
  [_omniboxContainer addSubview:editView];
  AddSameConstraints(_editView, _omniboxContainer);
}

#pragma mark - OmniboxPopupPresenterDelegate

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return _omniboxPopupContainer;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  _omniboxPopupContainer.hidden = NO;
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  _omniboxPopupContainer.hidden = YES;
}

#pragma mark - AIMInputItemCellDelegate

- (void)aimInputItemCellDidTapCloseButton:(AIMInputItemCell*)cell {
  NSIndexPath* indexPath = [_carouselView indexPathForCell:cell];
  if (indexPath) {
    AIMInputItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
    [self.mutator removeItem:item];
  }
}

#pragma mark - AIMPrototypeConsumer

- (void)setItems:(NSArray<AIMInputItem*>*)items {
  _carouselContainer.hidden = items.count == 0;
  NSDiffableDataSourceSnapshot<NSString*, AIMInputItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];
  __weak __typeof__(self) weakSelf = self;
  [_dataSource applySnapshot:snapshot
        animatingDifferences:YES
                  completion:^{
                    [weakSelf updateCarouselFade];
                  }];
}

- (void)updateState:(AIMInputItemState)state
    forItemWithToken:(const base::UnguessableToken&)token {
  NSDiffableDataSourceSnapshot<NSString*, AIMInputItem*>* currentSnapshot =
      _dataSource.snapshot;
  AIMInputItem* itemToUpdate;
  for (AIMInputItem* item in currentSnapshot.itemIdentifiers) {
    if (item.token == token) {
      itemToUpdate = item;
      break;
    }
  }

  if (!itemToUpdate) {
    return;
  }

  itemToUpdate.state = state;
  NSDiffableDataSourceSnapshot<NSString*, AIMInputItem*>* newSnapshot =
      [currentSnapshot copy];
  [newSnapshot reconfigureItemsWithIdentifiers:@[ itemToUpdate ]];
  [_dataSource applySnapshot:newSnapshot animatingDifferences:YES];
}

- (void)hideMicButton:(BOOL)hidden {
  _micButton.hidden = hidden;
}

#pragma mark - Actions

- (void)closeButtonTapped {
  [self.delegate aimPrototypeViewControllerDidTapCloseButton:self];
}

- (void)galleryButtonTapped {
  [self.delegate aimPrototypeViewControllerDidTapGalleryButton:self];
}

- (void)cameraButtonTapped {
  [self.delegate aimPrototypeViewControllerDidTapCameraButton:self];
}

- (void)aimButtonTapped {
  self.AIModeEnabled = !self.AIModeEnabled;
}

- (void)plusButtonTouchDown {
  [self.delegate aimPrototypeViewControllerMayShowGalleryPicker:self];
}

- (void)micButtonTapped {
  [self.delegate aimPrototypeViewControllerDidTapMicButton:self];
}

- (void)stopGlowEffect {
  [_glowEffectView stopGlow];
}

- (void)updateCarouselFade {
  // Checks if the carousel is scrollable.
  BOOL shouldShowFade =
      _carouselView.contentSize.width > _carouselView.bounds.size.width;
  _carouselFadeView.hidden = !shouldShowFade;
}

#pragma mark - Private

- (void)setAIModeEnabled:(BOOL)AIModeEnabled {
  if (AIModeEnabled == _AIModeEnabled) {
    return;
  }
  _AIModeEnabled = AIModeEnabled;
  [self updateAIMButtonAppearance];
  [self.mutator setAIModeEnabled:_AIModeEnabled];

  if (_AIModeEnabled && _glowEffectView) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self
                                             selector:@selector(stopGlowEffect)
                                               object:nil];
    [_glowEffectView startGlow];
    [self performSelector:@selector(stopGlowEffect)
               withObject:nil
               afterDelay:kGlowEffectDuration];
  }
}

- (UICollectionViewDiffableDataSource<NSString*, AIMInputItem*>*)
    createDataSource {
  return [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_carouselView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    AIMInputItem* item) {
                  AIMInputItemCell* cell = (AIMInputItemCell*)[collectionView
                      dequeueReusableCellWithReuseIdentifier:
                          kItemCellReuseIdentifier
                                                forIndexPath:indexPath];
                  [cell configureWithItem:item];
                  cell.delegate = self;
                  return cell;
                }];
}

- (void)updateAIMButtonAppearance {
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];

  // Font setup
  UIFont* font = [UIFont systemFontOfSize:kAIMButtonFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:@"AI Mode"
                                      attributes:attributes];
  config.attributedTitle = attributedTitle;

  config.contentInsets = NSDirectionalEdgeInsetsMake(5, 8, 5, 8);
  config.imagePadding = 5;
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

  if (self.AIModeEnabled) {
    config.image =
        DefaultSymbolWithPointSize(kCheckmarkSymbol, kAIMButtonSymbolPointSize);
    config.background.backgroundColor = [UIColor colorNamed:kBlue100Color];
    config.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  } else {
    config.image = CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                             kAIMButtonSymbolPointSize);
    config.background.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  _aimButton.configuration = config;
}

- (UIButton*)createButtonWithImage:(UIImage*)image {
  UIButton* button =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button.widthAnchor constraintEqualToConstant:kGenericButtonWidth].active =
      YES;
  [button.heightAnchor constraintEqualToConstant:kGenericButtonHeight].active =
      YES;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  return button;
}

@end
