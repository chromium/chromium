// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_view_controller.h"

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_cell.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_view.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_mutator.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
const CGFloat kCarouselItemSpacing = 6.0f;
/// The height of the carousel view.
const CGFloat kCarouselHeight = 36.0f;
/// The height of the AIM mode button.
const CGFloat kAIMButtonHeight = 36.0f;
/// The width of the AIM mode button.
const CGFloat kAIMButtonWidth = 94.0f;
/// The spacing for the horizontal buttons stack view.
const CGFloat kButtonsStackViewSpacing = 6.0f;
/// The spacing for the main vertical input plate stack view.
const CGFloat kInputPlateStackViewSpacing = 10.0f;
/// The vertical padding for the input plate stack view.
const CGFloat kInputPlateStackViewVerticalPadding = 10.0f;
/// The leading padding for the input plate stack view.
const CGFloat kInputPlateStackViewLeadingPadding = 10.0f;
/// The trailing padding for the input plate stack view.
const CGFloat kInputPlateStackViewTrailingPadding = 12.0f;
/// The font size for the AIM mode button title.
const CGFloat kAIMButtonFontSize = 14.0f;
/// The point size for the symbols in the AIM mode button.
const CGFloat kAIMButtonSymbolPointSize = 12;
/// The width of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonWidth = 24.0f;
/// The height of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonHeight = 32.0f;

/// The duration for the glow effect.
const CGFloat kGlowEffectDuration = 1.0f;
/// The width of the glow effect border.
const CGFloat kGlowEffectWidth = 40.0f;

/// The top padding between the omnibox container and the mic and lens button.
const CGFloat kMicLensButtonTopPadding = 2.0f;
/// The padding between the mic and lens buttons.
const CGFloat kMicLensButtonHorizontalPadding = 10.0f;
/// The trailing padding between the omnibox container and the mic button.
const CGFloat kLensButtonTrailingPadding = 5.0f;

/// The fade view width.
const CGFloat kFadeViewWidth = 30.0f;
/// The duration for the AIM button animation.
const CGFloat kAIMButtonAnimationDuration = 0.25f;
}  // namespace

@interface AIMPrototypeComposeboxViewController () <
    UITextViewDelegate,
    AIMInputItemCellDelegate,
    UICollectionViewDelegate,
    UICollectionViewDelegateFlowLayout>

/// Whether the AI mode is enabled.
@property(nonatomic, assign) BOOL AIModeEnabled;

/// Whether the carousel is scrollable.
@property(nonatomic, assign) BOOL shouldMinimizeAimButton;

/// The width constraint for the AIM button.
@property(nonatomic, strong) NSLayoutConstraint* aimButtonWidthConstraint;

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

@end

@implementation AIMPrototypeComposeboxViewController {
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
  /// The lens button.
  UIButton* _lensButton;
  /// The fade view for the carousel's leading edge.
  UIView* _leadingCarouselFadeView;
  /// The fade view for the carousel's trailing edge.
  UIView* _trailingCarouselFadeView;
  /// The carousel container.
  UIView* _carouselContainer;
  /// Wether or not the current tab is attachable.
  BOOL _canAttachCurrentTab;
  /// Container for the omnibox.
  UIView* _omniboxContainer;
  /// A spacer view used in the stack view.
  UIView* _spacerView;

  /// The cancellable callback for updating the glow effect.
  base::CancelableOnceClosure _updateGlowCallback;
}

/// AIMPrototypeAnimationContextProvider
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;

- (instancetype)init {
  self = [super init];
  if (self) {
    _omniboxContainer = [[UIView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

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
    AddSameConstraintsWithInset(_inputPlateContainerView, _glowEffectView,
                                kGlowEffectWidth);
  }

  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _micButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                       kSymbolActionPointSize)];
  [_micButton addTarget:self
                 action:@selector(micButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [_omniboxContainer addSubview:_micButton];

  _lensButton = [self
      createButtonWithImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                      kSymbolActionPointSize)];
  [_lensButton addTarget:self
                  action:@selector(lensButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [_omniboxContainer addSubview:_lensButton];

  AddSizeConstraints(_lensButton,
                     CGSizeMake(kGenericButtonWidth, kGenericButtonHeight));
  AddSizeConstraints(_micButton,
                     CGSizeMake(kGenericButtonWidth, kGenericButtonHeight));

  [NSLayoutConstraint activateConstraints:@[
    [_lensButton.topAnchor constraintEqualToAnchor:_omniboxContainer.topAnchor
                                          constant:kMicLensButtonTopPadding],
    [_lensButton.trailingAnchor
        constraintEqualToAnchor:_omniboxContainer.trailingAnchor
                       constant:-kLensButtonTrailingPadding],
    [_micButton.topAnchor constraintEqualToAnchor:_omniboxContainer.topAnchor
                                         constant:kMicLensButtonTopPadding],
    [_micButton.trailingAnchor
        constraintEqualToAnchor:_lensButton.leadingAnchor
                       constant:-kMicLensButtonHorizontalPadding],
  ]];

  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[AIMInputItemCell class]
      forCellWithReuseIdentifier:kItemCellReuseIdentifier];
  _dataSource = [self createDataSource];
  _carouselView.dataSource = _dataSource;
  _carouselView.delegate = self;
  [_carouselView.heightAnchor constraintEqualToConstant:kCarouselHeight]
      .active = YES;
  _carouselView.showsHorizontalScrollIndicator = NO;

  // Carousel container and fade view.
  _carouselContainer = [[UIView alloc] init];
  _carouselContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [_carouselContainer addSubview:_carouselView];
  AddSameConstraints(_carouselContainer, _carouselView);

  _trailingCarouselFadeView = [[UIView alloc] init];
  _trailingCarouselFadeView.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingCarouselFadeView.userInteractionEnabled = NO;
  _trailingCarouselFadeView.hidden = YES;
  [_carouselContainer addSubview:_trailingCarouselFadeView];

  _leadingCarouselFadeView = [[UIView alloc] init];
  _leadingCarouselFadeView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingCarouselFadeView.userInteractionEnabled = NO;
  _leadingCarouselFadeView.hidden = YES;
  [_carouselContainer addSubview:_leadingCarouselFadeView];

  [_trailingCarouselFadeView.layer
      insertSublayer:[self createGradientLayerForLeading:NO]
             atIndex:0];
  [_leadingCarouselFadeView.layer
      insertSublayer:[self createGradientLayerForLeading:YES]
             atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_trailingCarouselFadeView.trailingAnchor
        constraintEqualToAnchor:_carouselContainer.trailingAnchor],
    [_trailingCarouselFadeView.topAnchor
        constraintEqualToAnchor:_carouselContainer.topAnchor],
    [_trailingCarouselFadeView.bottomAnchor
        constraintEqualToAnchor:_carouselContainer.bottomAnchor],
    [_trailingCarouselFadeView.widthAnchor
        constraintEqualToConstant:kFadeViewWidth],

    [_leadingCarouselFadeView.leadingAnchor
        constraintEqualToAnchor:_carouselContainer.leadingAnchor],
    [_leadingCarouselFadeView.topAnchor
        constraintEqualToAnchor:_carouselContainer.topAnchor],
    [_leadingCarouselFadeView.bottomAnchor
        constraintEqualToAnchor:_carouselContainer.bottomAnchor],
    [_leadingCarouselFadeView.widthAnchor
        constraintEqualToConstant:kFadeViewWidth],
  ]];

  // Action buttons
  UIButton* plusButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [plusButton
      setImage:DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize)
      forState:UIControlStateNormal];
  plusButton.translatesAutoresizingMaskIntoConstraints = NO;
  plusButton.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  plusButton.layer.cornerRadius = kAIMButtonHeight / 2.0;
  plusButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];

  [plusButton.widthAnchor constraintEqualToConstant:kAIMButtonHeight].active =
      YES;
  [plusButton.heightAnchor constraintEqualToConstant:kAIMButtonHeight].active =
      YES;

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

  NSMutableArray* menuItems = [NSMutableArray
      arrayWithObjects:fileAction, galleryAction, cameraAction, nil];

  if (base::FeatureList::IsEnabled(kAIMPrototypeTabPicker)) {
    UIAction* selectTabsAction = [UIAction
        // TODO(crbug.com/40280872): Localize this string.
        actionWithTitle:@"Attach tabs"
                  image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf handleAttachTabs];
                }];
    [menuItems addObject:selectTabsAction];
  }

  if (_canAttachCurrentTab &&
      !base::FeatureList::IsEnabled(kAIMPrototypeTabPicker)) {
    [menuItems addObject:attachCurrentTabAction];
  }

  plusButton.menu = [UIMenu menuWithTitle:@"" children:menuItems];

  _aimButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _aimButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_aimButton addTarget:self
                 action:@selector(aimButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [self updateAIMButtonAppearance];

  [_aimButton.heightAnchor constraintEqualToConstant:kAIMButtonHeight].active =
      YES;
  self.aimButtonWidthConstraint =
      [_aimButton.widthAnchor constraintEqualToConstant:kAIMButtonWidth];
  self.aimButtonWidthConstraint.active = YES;

  // Horizontal stack view for buttons
  _spacerView = [[UIView alloc] init];
  [_spacerView setContentHuggingPriority:UILayoutPriorityFittingSizeLevel
                                 forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        plusButton, _carouselContainer, _spacerView, _aimButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = kButtonsStackViewSpacing;
  buttonsStackView.alignment = UIStackViewAlignmentBottom;

  // Main vertical stack view
  _inputPlateStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _omniboxContainer, buttonsStackView ]];
  _inputPlateStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlateStackView.axis = UILayoutConstraintAxisVertical;
  _inputPlateStackView.spacing = kInputPlateStackViewSpacing;
  [_inputPlateContainerView addSubview:_inputPlateStackView];

  AddSameConstraints(_inputPlateContainerView, self.view);

  // Layout.
  [NSLayoutConstraint activateConstraints:@[
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
  _trailingCarouselFadeView.layer.sublayers.firstObject.frame =
      _trailingCarouselFadeView.bounds;
  _leadingCarouselFadeView.layer.sublayers.firstObject.frame =
      _leadingCarouselFadeView.bounds;
  [self updateInputPlateLayout];
}

#pragma mark - Public

- (CGFloat)inputHeight {
  return _inputPlateContainerView.frame.size.height;
}

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  _editView = editView;
  _editView.translatesAutoresizingMaskIntoConstraints = NO;
  [_omniboxContainer addSubview:editView];
  AddSameConstraints(_editView, _omniboxContainer);
}

#pragma mark - AIMInputItemCellDelegate

- (void)aimInputItemCellDidTapCloseButton:(AIMInputItemCell*)cell {
  NSIndexPath* indexPath = [_carouselView indexPathForCell:cell];
  if (indexPath) {
    AIMInputItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
    [self.mutator removeItem:item];
  }
}

#pragma mark - AIMPrototypeComposeboxConsumer

- (void)setItems:(NSArray<AIMInputItem*>*)items {
  NSDiffableDataSourceSnapshot<NSString*, AIMInputItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];
  __weak __typeof__(self) weakSelf = self;
  [_dataSource applySnapshot:snapshot
        animatingDifferences:YES
                  completion:^{
                    [weakSelf updateInputPlateLayout];
                  }];
}

- (void)setCanAttachTabAction:(BOOL)canAttachTabAction {
  _canAttachCurrentTab = canAttachTabAction;
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

- (void)hideLensAndMicButton:(BOOL)hidden {
  _micButton.hidden = hidden;
  _lensButton.hidden = hidden;
}

#pragma mark - Actions

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
  [self.delegate aimPrototypeViewController:self didTapMicButton:_micButton];
}

- (void)lensButtonTapped {
  [self.delegate aimPrototypeViewController:self didTapLensButton:_lensButton];
}

- (void)stopGlowEffect {
  [_glowEffectView stopGlow];
}

- (void)updateInputPlateLayout {
  // Determine if the AIM button should be minimized.
  BOOL shouldMinimize = self.shouldMinimizeAimButton;

  BOOL isCarouselScrollable =
      _carouselView.contentSize.width > _carouselView.bounds.size.width;
  if (isCarouselScrollable) {
    // If the carousel is scrollable, the button must be minimized.
    shouldMinimize = YES;
  } else {
    // Calculate the unused space available within the carousel's frame.
    CGFloat availableSpace =
        _carouselView.bounds.size.width - _carouselView.contentSize.width;
    // Calculate the extra width the AIM button needs to change from its
    // minimized (circular) to its full expanded state.
    CGFloat expansionWidthNeeded = kAIMButtonWidth - kAIMButtonHeight;

    // If the available space is greater than the space needed for the button
    // to expand, the button should not be minimized.
    if (availableSpace > expansionWidthNeeded) {
      shouldMinimize = NO;
    }
  }

  // If the minimization state has changed, update the button's appearance and
  // animate the layout change.
  if (self.shouldMinimizeAimButton != shouldMinimize) {
    _shouldMinimizeAimButton = shouldMinimize;
    [self updateAIMButtonAppearance];
    if (shouldMinimize) {
      [UIView animateWithDuration:kAIMButtonAnimationDuration
                       animations:^{
                         [self.view layoutIfNeeded];
                       }];
    }
  }

  [self updateCarouselFade];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  AIMInputItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];

  if (!item || item.type == AIMInputItemType::kAIMInputItemTypeImage) {
    return kImageInputItemSize;
  }

  return kTabFileInputItemSize;
}

#pragma mark - Private

- (void)handleAttachTabs {
  [self.delegate aimPrototypeViewControllerDidTapAttachTabsButton:self];
}

- (void)updateCarouselFade {
  CGFloat contentOffsetX = _carouselView.contentOffset.x;
  CGFloat contentWidth = _carouselView.contentSize.width;
  CGFloat boundsWidth = _carouselView.bounds.size.width;

  _leadingCarouselFadeView.hidden = contentOffsetX <= 0;
  _trailingCarouselFadeView.hidden =
      contentOffsetX + boundsWidth >= contentWidth;
}

- (void)setAIModeEnabled:(BOOL)AIModeEnabled {
  if (AIModeEnabled == _AIModeEnabled) {
    return;
  }
  _AIModeEnabled = AIModeEnabled;
  [self updateAIMButtonAppearance];
  [self.mutator setAIModeEnabled:_AIModeEnabled];
  [self triggerGlowEffect];
}

- (void)triggerGlowEffect {
  if (!_glowEffectView) {
    return;
  }

  // Cancel any previously scheduled updates.
  _updateGlowCallback.Cancel();

  if (_AIModeEnabled) {
    // When turning on, ensure the glow is started. The view's state machine
    // will prevent it from restarting if it's already active.
    [_glowEffectView startGlow];
  } else if (_glowEffectView.glowState == GlowState::kStoppingRotation) {
    // If the user toggles off while the rotation is already stopping, stop the
    // glow immediately.
    [_glowEffectView stopGlow];
    return;
  }

  // Schedule the next state transition after the delay, regardless of whether
  // the mode was turned on or off.
  __weak __typeof__(self) weakSelf = self;
  _updateGlowCallback.Reset(base::BindOnce(^{
    [weakSelf updateGlow];
  }));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, _updateGlowCallback.callback(),
      base::Seconds(kGlowEffectDuration));
}

/// Called after a delay to transition the glow effect to its next state.
- (void)updateGlow {
  if (self.AIModeEnabled) {
    // If the mode is still enabled, stop the rotation but keep the glow.
    [_glowEffectView stopRotation];
  } else {
    // If the mode has been disabled, stop the glow entirely.
    [_glowEffectView stopGlow];
  }
}

#pragma mark - UICollectionViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateCarouselFade];
}

- (CAGradientLayer*)createGradientLayerForLeading:(BOOL)isLeading {
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  UIColor* transparentColor = [[UIColor colorNamed:kPrimaryBackgroundColor]
      colorWithAlphaComponent:0.0];
  UIColor* solidColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  if (isLeading) {
    gradientLayer.colors =
        @[ (id)solidColor.CGColor, (id)transparentColor.CGColor ];
  } else {
    gradientLayer.colors =
        @[ (id)transparentColor.CGColor, (id)solidColor.CGColor ];
  }

  gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  return gradientLayer;
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

  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.image = CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                           kAIMButtonSymbolPointSize);

  if (self.shouldMinimizeAimButton) {
    self.aimButtonWidthConstraint.constant = kAIMButtonHeight;
  } else {
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
    self.aimButtonWidthConstraint.constant = kAIMButtonWidth;
  }

  if (self.AIModeEnabled) {
    config.background.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
    config.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  } else {
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
