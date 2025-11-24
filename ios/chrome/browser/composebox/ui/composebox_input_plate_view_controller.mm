// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/composebox/ui/composebox_animation_context_provider.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_cell.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_view.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"
#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// The reuse identifier for the input item cells in the carousel.
NSString* const kItemCellReuseIdentifier = @"ComposeboxInputItemCell";
/// The identifier for the main section of the collection view.
NSString* const kMainSectionIdentifier = @"MainSection";

/// The corner radius for the input plate container.
const CGFloat kInputPlateCornerRadius = 22.0f;
/// The shadow opacity for the input plate container.
const float kInputPlateShadowOpacity = 0.2f;
/// The shadow radius for the input plate container.
const CGFloat kInputPlateShadowRadius = 20.0f;
/// The spacing between items in the carousel.
const CGFloat kCarouselItemSpacing = 6.0f;
/// The height of the carousel view.
const CGFloat kCarouselHeight = 44.0f;
/// The height of the AIM mode button.
const CGFloat kAIMButtonHeight = 36.0f;
/// The width of the AIM mode button.
const CGFloat kAIMButtonWidth = 122.0f;
/// The spacing for the horizontal buttons stack view.
const CGFloat kButtonsCompactSpacing = 4.0f;
const CGFloat kButtonsStackViewSpacing = 6.0f;
/// The spacing between the Lens and Voice buttons.
const CGFloat kShortcutsSpacing = 24.0f;
const CGFloat kShortcutsSpacingCompact = 16.0f;
/// The spacing for the main vertical input plate stack view.
const CGFloat kInputPlateStackViewSpacing = 10.0f;
/// The vertical padding for the input plate stack view.
const CGFloat kInputPlateStackViewVerticalCompactPadding = 6.0f;
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
/// The dimension of the send button.
const CGFloat kSendButtonDimension = 32.0f;

/// The duration for the glow effect.
const CGFloat kGlowEffectDuration = 0.9;
/// The width of the glow effect border.
const CGFloat kGlowEffectWidth = 2.0f;
/// Duration of a change in compact mode.
const CGFloat kCompactModeAnimationDuration = 0.1;
/// The opacity once the send button is disabled.
const CGFloat kSendButtonDisabledOpacity = 0.5;
/// The fade view width.
const CGFloat kFadeViewWidth = 30.0f;

/// The size of the close icon in the context indicator buttons.
const CGFloat kCloseIndicatorSize = 10.0f;
}  // namespace


@interface ComposeboxInputPlateViewController () <
    UITextViewDelegate,
    ComposeboxInputItemCellDelegate,
    UICollectionViewDelegate,
    UICollectionViewDelegateFlowLayout>

/// Whether the AI mode is enabled.
@property(nonatomic, assign) BOOL AIModeEnabled;

/// The width constraint for the AIM button.
@property(nonatomic, strong) NSLayoutConstraint* aimButtonWidthConstraint;

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

/// Whether the UI is in compact (single line) mode.
@property(nonatomic, assign) BOOL isCompactMode;

/// The send button.
@property(nonatomic, strong) UIButton* sendButton;

/// The stack view for the input plate.
@property(nonatomic, strong) UIStackView* inputPlateStackView;

@end

@implementation ComposeboxInputPlateViewController {
  /// The collection view to display the selected images.
  UICollectionView* _carouselView;
  /// The diffable data source for the carousel view.
  UICollectionViewDiffableDataSource<NSString*, ComposeboxInputItem*>*
      _dataSource;
  /// The view containing the input text field and action buttons.
  UIView* _inputPlateContainerView;
  /// The button to toggle AI mode.
  UIButton* _aimButton;
  UIImageView* _aimButtonXIndicator;
  /// The glow effect around the input plate container.
  UIView<GlowEffect>* _glowEffectView;
  /// The plus button.
  UIButton* _plusButton;
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

  /// The cancellable callback for updating the glow effect.
  base::CancelableOnceClosure _updateGlowCallback;

  // The theme of the composebox.
  ComposeboxTheme* _theme;

  // The favicon for the current tab.
  UIImage* _currentTabFavicon;

  // Constraints for the dynamic padding of the input plate stack view.
  NSLayoutConstraint* _topPaddingConstraint;
  NSLayoutConstraint* _bottomPaddingConstraint;
}

/// ComposeboxAnimationContextProvider
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;

- (instancetype)initWithTheme:(ComposeboxTheme*)theme {
  self = [super init];
  if (self) {
    _omniboxContainer = [[UIView alloc] init];
    _theme = theme;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // --- Bottom Input Area ---

  // Input plate container
  [self setupInputPlateContainerView];
  AddSameConstraints(_inputPlateContainerView, self.view);

  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _micButton = [self createMicrophoneButton];
  _lensButton = [self createLensButton];
  _plusButton = [self createPlusButton];
  _sendButton = [self createSendButton];
  [self updatePlusButtonItems];
  [self setupCarouselContainer];

  _inputPlateStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _omniboxContainer ]];
  _inputPlateStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_inputPlateContainerView addSubview:_inputPlateStackView];

  _bottomPaddingConstraint = [_inputPlateStackView.bottomAnchor
      constraintEqualToAnchor:_inputPlateContainerView.bottomAnchor
                     constant:-kInputPlateStackViewVerticalCompactPadding];
  [NSLayoutConstraint activateConstraints:@[ _bottomPaddingConstraint ]];

  AddSameConstraintsToSidesWithInsets(
      _inputPlateStackView, _inputPlateContainerView,
      (LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing),
      NSDirectionalEdgeInsetsMake(kInputPlateStackViewVerticalCompactPadding,
                                  kInputPlateStackViewLeadingPadding, 0,
                                  kInputPlateStackViewTrailingPadding));

  [self updateInputPlateStackViewAnimated:NO];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(userInterfaceStyleChanged)];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update the gradient layer's frame.
  _trailingCarouselFadeView.layer.sublayers.firstObject.frame =
      _trailingCarouselFadeView.bounds;
  _leadingCarouselFadeView.layer.sublayers.firstObject.frame =
      _leadingCarouselFadeView.bounds;
  [self updateCarouselFade];
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

#pragma mark - ComposeboxInputItemCellDelegate

- (void)composeboxInputItemCellDidTapCloseButton:
    (ComposeboxInputItemCell*)cell {
  NSIndexPath* indexPath = [_carouselView indexPathForCell:cell];
  if (indexPath) {
    ComposeboxInputItem* item =
        [_dataSource itemIdentifierForIndexPath:indexPath];
    [self.mutator removeItem:item];
  }
}

#pragma mark - ComposeboxInputPlateConsumer

- (void)setItems:(NSArray<ComposeboxInputItem*>*)items {
  _carouselContainer.hidden = !items.count;
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxInputItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];
  __weak __typeof__(self) weakSelf = self;
  [_dataSource applySnapshot:snapshot
        animatingDifferences:YES
                  completion:^{
                    [weakSelf updateCarouselFade];
                    [weakSelf updateSendButtonStateIfNeeded];
                  }];
}

- (void)setCanAttachCurrentTab:(BOOL)canAttachCurrentTab {
  if (_canAttachCurrentTab == canAttachCurrentTab) {
    return;
  }
  _canAttachCurrentTab = canAttachCurrentTab;
  [self updatePlusButtonItems];
}

- (void)updateState:(ComposeboxInputItemState)state
    forItemWithToken:(const base::UnguessableToken&)token {
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxInputItem*>*
      currentSnapshot = _dataSource.snapshot;
  ComposeboxInputItem* itemToUpdate;
  for (ComposeboxInputItem* item in currentSnapshot.itemIdentifiers) {
    if (item.token == token) {
      itemToUpdate = item;
      break;
    }
  }

  if (!itemToUpdate) {
    return;
  }

  itemToUpdate.state = state;
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxInputItem*>* newSnapshot =
      [currentSnapshot copy];
  [newSnapshot reconfigureItemsWithIdentifiers:@[ itemToUpdate ]];
  __weak __typeof__(self) weakSelf = self;
  [_dataSource applySnapshot:newSnapshot
        animatingDifferences:YES
                  completion:^{
                    [weakSelf updateSendButtonStateIfNeeded];
                  }];
}

- (void)updateSendButtonStateIfNeeded {
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxInputItem*>*
      currentSnapshot = _dataSource.snapshot;

  BOOL allLoaded = YES;
  for (ComposeboxInputItem* item in currentSnapshot.itemIdentifiers) {
    if (item.state != ComposeboxInputItemState::kLoaded) {
      allLoaded = NO;
      break;
    }
  }

  [self enableSendButton:allLoaded];
}

- (void)enableSendButton:(BOOL)enableSending {
  if (enableSending) {
    _sendButton.alpha = 1;
    _sendButton.enabled = YES;
    [_editView forceDisableReturnKey:NO];
  } else {
    _sendButton.alpha = kSendButtonDisabledOpacity;
    _sendButton.enabled = NO;
    [_editView forceDisableReturnKey:YES];
  }
}

- (void)hideLensAndMicButton:(BOOL)hidden {
  _micButton.hidden = hidden;
  _lensButton.hidden = hidden;
}

- (void)hideSendButton:(BOOL)hidden {
  _sendButton.hidden = hidden;
}

- (void)setIsCompactMode:(BOOL)isCompactMode {
  if (_isCompactMode == isCompactMode) {
    return;
  }
  _isCompactMode = isCompactMode;

  if (!self.viewLoaded) {
    return;
  }

  [self updateInputPlateStackViewAnimated:YES];
}

- (void)setCurrentTabFavicon:(UIImage*)favicon {
  _currentTabFavicon = favicon;
  [self updatePlusButtonItems];
}

#pragma mark - Actions

- (void)galleryButtonTapped {
  [self.delegate composeboxViewControllerDidTapGalleryButton:self];
}

- (void)cameraButtonTapped {
  [self.delegate composeboxViewControllerDidTapCameraButton:self];
}

- (void)aimButtonTapped {
  self.AIModeEnabled = !self.AIModeEnabled;
  if (self.AIModeEnabled) {
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kDedicatedButton];
  }
}

- (void)plusButtonTouchDown {
  [self.delegate composeboxViewControllerMayShowGalleryPicker:self];
}

- (void)micButtonTapped {
  [self.delegate composeboxViewController:self didTapMicButton:_micButton];
}

- (void)lensButtonTapped {
  [self.delegate composeboxViewController:self didTapLensButton:_lensButton];
}

- (void)sendButtonTapped {
  [self.delegate composeboxViewController:self didTapSendButton:_sendButton];
}

- (void)stopGlowEffect {
  [_glowEffectView stopGlow];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  ComposeboxInputItem* item =
      [_dataSource itemIdentifierForIndexPath:indexPath];

  if (!item ||
      item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage) {
    return composeboxAttachments::kImageInputItemSize;
  }

  return composeboxAttachments::kTabFileInputItemSize;
}

#pragma mark - Private

- (void)handleAttachTabs {
  [self.delegate composeboxViewControllerDidTapAttachTabsButton:self];
}

- (void)handleAIMTappedFromToolMenu {
  self.AIModeEnabled = !self.AIModeEnabled;
  if (self.AIModeEnabled) {
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kToolMenu];
  }
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
  [self updatePlusButtonItems];
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
  [_glowEffectView stopGlow];
}

- (void)userInterfaceStyleChanged {
  [self updateAIMButtonAppearance];
  [self updateDepthShadowAppearance];
}

#pragma mark - UICollectionViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateCarouselFade];
}

- (CAGradientLayer*)createGradientLayerForLeading:(BOOL)isLeading {
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  UIColor* transparentColor =
      [_theme.inputPlateBackgroundColor colorWithAlphaComponent:0.0];
  UIColor* solidColor = _theme.inputPlateBackgroundColor;

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

- (UICollectionViewDiffableDataSource<NSString*, ComposeboxInputItem*>*)
    createDataSource {
  __weak ComposeboxTheme* theme = _theme;
  return [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_carouselView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    ComposeboxInputItem* item) {
                  ComposeboxInputItemCell* cell =
                      (ComposeboxInputItemCell*)[collectionView
                          dequeueReusableCellWithReuseIdentifier:
                              kItemCellReuseIdentifier
                                                    forIndexPath:indexPath];
                  [cell configureWithItem:item theme:theme];
                  cell.delegate = self;
                  return cell;
                }];
}

#pragma mark - Private helpers

- (void)updateDepthShadowAppearance {
  if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark ||
      _theme.isTopInputPlate) {
    _inputPlateContainerView.layer.shadowOpacity = 0;
  } else {
    _inputPlateContainerView.layer.shadowColor =
        [UIColor colorNamed:kTextPrimaryColor].CGColor;
    _inputPlateContainerView.layer.shadowRadius = kInputPlateShadowRadius;
    _inputPlateContainerView.layer.shadowOffset = CGSizeZero;
    _inputPlateContainerView.layer.shadowOpacity = kInputPlateShadowOpacity;
  }

  [_inputPlateContainerView.layer setNeedsDisplay];
}

/// Updates the AIM button taking into account if the button should be minimize
/// or not or if the mode is enable or not.
- (void)updateAIMButtonAppearance {
  if (!_aimButton) {
    return;
  }

  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];

  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.image = CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                           kAIMButtonSymbolPointSize);

  // Font setup
  UIFont* font = [UIFont systemFontOfSize:kAIMButtonFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:@"AI Mode"
                                      attributes:attributes];
  config.attributedTitle = attributedTitle;

  config.imagePadding = 5;
  self.aimButtonWidthConstraint.constant = kAIMButtonWidth;
  _aimButton.layer.borderWidth = 0;

  if (self.AIModeEnabled) {
    config.contentInsets = NSDirectionalEdgeInsetsMake(5, 8, 5, 22);
    config.background.backgroundColor =
        [_theme aimButtonBackgroundColorWithAIMEnabled:YES];
    config.baseForegroundColor = [_theme aimButtonTextColorWithAIMEnabled:YES];

    _aimButton.hidden = NO;
  } else {
    config.contentInsets = NSDirectionalEdgeInsetsMake(5, 8, 5, 8);
    config.background.backgroundColor =
        [_theme aimButtonBackgroundColorWithAIMEnabled:NO];
    config.baseForegroundColor = [_theme aimButtonTextColorWithAIMEnabled:NO];

    if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
      _aimButton.layer.borderWidth = 1;
      _aimButton.layer.borderColor = [UIColor colorNamed:kGrey200Color].CGColor;
    }

    _aimButton.hidden = YES;
  }

  _aimButton.configuration = config;

  // Setup the X mark only after the config was aplied, otherwise the
  // constraints applied relative to the title label will be wrong for iOS 18.
  if (self.AIModeEnabled) {
    [self setupXMarkInAIMButton];
  } else {
    [_aimButtonXIndicator removeFromSuperview];
    _aimButtonXIndicator = nil;
  }
}

- (void)setupXMarkInAIMButton {
  [_aimButtonXIndicator removeFromSuperview];

  _aimButtonXIndicator = [[UIImageView alloc] init];
  _aimButtonXIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseIndicatorSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  _aimButtonXIndicator.image =
      DefaultSymbolWithConfiguration(kXMarkSymbol, configuration);
  [_aimButton addSubview:_aimButtonXIndicator];

  [NSLayoutConstraint activateConstraints:@[
    [_aimButton.titleLabel.trailingAnchor
        constraintEqualToAnchor:_aimButtonXIndicator.leadingAnchor
                       constant:-4],
    [_aimButton.titleLabel.centerYAnchor
        constraintEqualToAnchor:_aimButtonXIndicator.centerYAnchor],
  ]];
}

/// Creates an extended touch target button with the given image.
- (UIButton*)createButtonWithImage:(UIImage*)image {
  UIButton* button =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button.widthAnchor constraintEqualToConstant:kGenericButtonWidth].active =
      YES;
  [button.heightAnchor constraintEqualToConstant:kGenericButtonHeight].active =
      YES;
  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  return button;
}

/// Creates the plus button that contains the menu.
- (UIButton*)createPlusButton {
  UIButton* plusButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [plusButton
      setImage:DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize)
      forState:UIControlStateNormal];
  plusButton.translatesAutoresizingMaskIntoConstraints = NO;
  plusButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];

  AddSizeConstraints(plusButton,
                     CGSizeMake(kAIMButtonHeight, kAIMButtonHeight));

  [plusButton addTarget:self
                 action:@selector(plusButtonTouchDown)
       forControlEvents:UIControlEventTouchDown];
  plusButton.showsMenuAsPrimaryAction = YES;

  return plusButton;
}

/// Returns the send button.
- (UIButton*)createSendButton {
  UIButton* sendButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:24
                          weight:UIImageSymbolWeightSemibold];

  UIImage* image = SymbolWithPalette(
      DefaultSymbolWithConfiguration(kRightArrowCircleFillSymbol, config), @[
        [UIColor colorNamed:kSolidWhiteColor],
        [UIColor colorNamed:kBlue500Color]
      ]);
  [sendButton setImage:image forState:UIControlStateNormal];

  [sendButton addTarget:self
                 action:@selector(sendButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  AddSizeConstraints(sendButton,
                     CGSizeMake(kSendButtonDimension, kSendButtonDimension));
  return sendButton;
}

/// Returns the microphone button.
- (UIButton*)createMicrophoneButton {
  UIButton* micButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                       kSymbolActionPointSize)];
  [micButton addTarget:self
                action:@selector(micButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  AddSizeConstraints(micButton,
                     CGSizeMake(kGenericButtonWidth, kGenericButtonHeight));
  return micButton;
}

/// Returns the lens button.
- (UIButton*)createLensButton {
  UIButton* lensButton = [self
      createButtonWithImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                      kSymbolActionPointSize)];
  [lensButton addTarget:self
                 action:@selector(lensButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];

  AddSizeConstraints(lensButton,
                     CGSizeMake(kGenericButtonWidth, kGenericButtonHeight));
  return lensButton;
}

- (UIView*)createToolbarView {
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
  UIView* spacerView = [[UIView alloc] init];
  [spacerView setContentHuggingPriority:UILayoutPriorityFittingSizeLevel
                                forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _plusButton, _aimButton, spacerView, _sendButton, _micButton,
        _lensButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [buttonsStackView setCustomSpacing:kShortcutsSpacing afterView:_micButton];
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = kButtonsStackViewSpacing;
  buttonsStackView.alignment = UIStackViewAlignmentFill;

  return buttonsStackView;
}

- (void)updatePlusButtonItems {
  if (!_plusButton) {
    return;
  }

  __weak __typeof__(self) weakSelf = self;
  UIAction* galleryAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
                image:DefaultSymbolWithPointSize(kPhotoSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    composeboxViewControllerDidTapGalleryButton:weakSelf];
              }];
  UIAction* cameraAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
                image:DefaultSymbolWithPointSize(kSystemCameraSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    composeboxViewControllerDidTapCameraButton:weakSelf];
              }];

  UIAction* fileAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
                image:DefaultSymbolWithPointSize(kDocSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    composeboxViewControllerDidTapFileButton:weakSelf];
              }];

  UIAction* attachCurrentTabAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION)
                          image:_currentTabFavicon
                                    ?: DefaultSymbolWithPointSize(
                                           kNewTabGroupActionSymbol,
                                           kSymbolActionPointSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator attachCurrentTabContent];
                        }];

  UIAction* selectTabsAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
                image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleAttachTabs];
              }];

  UIAction* aimAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION)
                image:CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                                kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleAIMTappedFromToolMenu];
              }];

  if (self.AIModeEnabled) {
    [aimAction setState:UIMenuElementStateOn];
  }

  UIAction* createImageAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION)
                          image:[self bananaIcon]
                     identifier:nil
                        handler:^(UIAction* action){
                        }];

  NSMutableArray* menuItems = [[NSMutableArray alloc] init];
  if (_canAttachCurrentTab) {
    [menuItems addObject:attachCurrentTabAction];
  }
  [menuItems addObjectsFromArray:@[
    selectTabsAction, cameraAction, galleryAction, fileAction
  ]];

  UIMenu* submenu = [UIMenu menuWithTitle:@""
                                    image:nil
                               identifier:nil
                                  options:UIMenuOptionsDisplayInline
                                 children:@[ aimAction, createImageAction ]];
  [menuItems addObject:submenu];

  _plusButton.menu = [UIMenu menuWithTitle:@"" children:menuItems];
}

- (void)setupCarouselContainer {
  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[ComposeboxInputItemCell class]
      forCellWithReuseIdentifier:kItemCellReuseIdentifier];
  _dataSource = [self createDataSource];
  _carouselView.dataSource = _dataSource;
  _carouselView.delegate = self;
  [_carouselView.heightAnchor constraintEqualToConstant:kCarouselHeight]
      .active = YES;
  _carouselView.showsHorizontalScrollIndicator = NO;

  _carouselContainer = [[UIView alloc] init];
  _carouselContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [_carouselContainer addSubview:_carouselView];
  _carouselContainer.hidden = YES;
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
}

- (void)setupInputPlateContainerView {
  _inputPlateContainerView = [[UIView alloc] init];
  _inputPlateContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlateContainerView.backgroundColor = _theme.inputPlateBackgroundColor;
  _inputPlateContainerView.layer.cornerRadius = kInputPlateCornerRadius;

  [self updateDepthShadowAppearance];
  [self.view addSubview:_inputPlateContainerView];

  _glowEffectView = ios::provider::CreateGlowEffect(
      CGRectZero, kInputPlateCornerRadius, kGlowEffectWidth);
  if (_glowEffectView) {
    _glowEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    _glowEffectView.userInteractionEnabled = NO;
    [self.view insertSubview:_glowEffectView
                aboveSubview:_inputPlateContainerView];
    AddSameConstraintsWithInset(_inputPlateContainerView, _glowEffectView,
                                kGlowEffectWidth);
  }
}

- (void)updateInputPlateStackViewContent {
  for (UIView* arrangedSubview in _inputPlateStackView.arrangedSubviews) {
    if (arrangedSubview != _omniboxContainer) {
      [_inputPlateStackView removeArrangedSubview:arrangedSubview];
      [arrangedSubview removeFromSuperview];
    }
  }

  if (self.isCompactMode) {
    [_inputPlateStackView insertArrangedSubview:_plusButton atIndex:0];
    [_inputPlateStackView addArrangedSubview:_micButton];
    [_inputPlateStackView addArrangedSubview:_lensButton];

    _inputPlateStackView.axis = UILayoutConstraintAxisHorizontal;
    _inputPlateStackView.spacing = 0;
    [_inputPlateStackView setCustomSpacing:kButtonsCompactSpacing
                                 afterView:_plusButton];
    [_inputPlateStackView setCustomSpacing:kShortcutsSpacingCompact
                                 afterView:_micButton];
    _bottomPaddingConstraint.constant =
        -kInputPlateStackViewVerticalCompactPadding;
  } else {
    UIView* toolbarView = [self createToolbarView];
    [_inputPlateStackView insertArrangedSubview:_carouselContainer atIndex:0];
    [_inputPlateStackView addArrangedSubview:toolbarView];
    _inputPlateStackView.axis = UILayoutConstraintAxisVertical;
    _inputPlateStackView.spacing = kInputPlateStackViewSpacing;

    _bottomPaddingConstraint.constant = -kInputPlateStackViewVerticalPadding;
  }
}

- (void)updateInputPlateStackViewAnimated:(BOOL)animated {
  if (!animated) {
    [self updateInputPlateStackViewContent];
    [self.editView hideLeadingImage:self.isCompactMode];
    return;
  }

  CGFloat initialAlpha = self.isCompactMode ? 1 : 0;
  CGFloat finalAlpha = 1 - initialAlpha;
  [self.editView setLeadingImageAlpha:initialAlpha];
  self.sendButton.alpha = initialAlpha;

  [self.editView hideLeadingImage:self.isCompactMode];

  auto animations = ^() {
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:1.0
                                  animations:^{
                                    [self updateInputPlateStackViewContent];
                                    [self.editView
                                        hideLeadingImage:self.isCompactMode];
                                    [self.inputPlateStackView layoutIfNeeded];
                                    [self.view layoutIfNeeded];
                                  }];
    [UIView
        addKeyframeWithRelativeStartTime:0.6
                        relativeDuration:0.4
                              animations:^{
                                [self.editView setLeadingImageAlpha:finalAlpha];
                                self.sendButton.alpha = finalAlpha;
                              }];
  };

  auto animationOptions = UIViewKeyframeAnimationOptionCalculationModeLinear;
  [UIView animateKeyframesWithDuration:kCompactModeAnimationDuration
                                 delay:0
                               options:animationOptions
                            animations:animations
                            completion:nil];
}

- (UIImage*)bananaIcon {
  CGFloat iconPadding = 4.0;
  CGSize size = CGSizeMake(kSymbolActionPointSize + iconPadding,
                           kSymbolActionPointSize + iconPadding);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size];
  UIImage* image = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        CGRect rect = CGRectMake(0, 0, size.width, size.height);
        UIFont* font = [UIFont systemFontOfSize:kSymbolActionPointSize];
        NSDictionary* attributes = @{
          NSFontAttributeName : font,
          NSForegroundColorAttributeName : UIColor.blackColor
        };
        [@"🍌" drawInRect:rect withAttributes:attributes];
      }];

  return image;
}

@end
