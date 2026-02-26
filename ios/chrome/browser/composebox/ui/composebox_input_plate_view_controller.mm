// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/cancelable_callback.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/composebox/public/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_animation_context.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_cell.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_view.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller_delegate.h"
#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_server_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_snackbar_presenter.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// The types of strings that can be shown in the interface.
enum class InputPlateString {
  kAIM,
  kImageGeneration,
  kCanvas,
  kDeepSearch,
  kRegularModel,
  kAutoModel,
  kThinkingModel,
  kToolsSection,
  kModelsSection,
};

// The types of strings that can be shown in the interface.
enum class InputPlateStringType {
  // A menu label string.
  kMenuLabel,
  // A chip label string.
  kChipLabel,
  // A hint text.
  kHintText,
};

/// The reuse identifier for the input item cells in the carousel.
NSString* const kItemCellReuseIdentifier = @"ComposeboxInputItemCell";
/// The identifier for the main section of the collection view.
NSString* const kMainSectionIdentifier = @"MainSection";

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
/// The corner radius of the favicon in attach current tab action.
const CGFloat kAttachCurrentTabIconRadius = 2.0f;
/// The width of the AIM mode button.
const CGFloat kAIMButtonBaseWidth = 108.0f;
const CGFloat kXButtonWidthInButton = 14.0;
const NSDirectionalEdgeInsets kModeIndicatorButtonInsets = {5, 8, 5, 8};
const NSDirectionalEdgeInsets kImageGenerationButtonInsets = {5, 8, 5, 28};
/// The spacing for the horizontal buttons stack view.
const CGFloat kButtonsCompactSpacing = 4.0f;
const CGFloat kButtonsStackViewSpacing = 6.0f;
/// The spacing between the Lens and Voice buttons.
const CGFloat kShortcutsSpacing = 10.0f;
/// The spacing for the main vertical input plate stack view.
const CGFloat kInputPlateStackViewSpacing = 6.0f;
/// The default vertical padding for the input plate. When the text view is the
/// top most element the padding must be 0. Otherwise, it won't extend to the
/// top edge when scrolling (crbug.com/464259064).
const CGFloat kInputPlateStackViewVerticalPadding = 0.0f;
/// The top padding with the expanded input plate when there are attachments.
const CGFloat kInputPlateStackViewExpandedWithAttachmentsTopPadding = 10.0f;
/// The bottom padding with the expanded input plate when AIM is available.
const CGFloat kInputPlateStackViewExpandedBottomPadding = 10.0f;
/// The horizontal padding for the input plate stack view.
const NSDirectionalEdgeInsets kInputPlateStackViewPadding = {.leading = 0.0f,
                                                             .trailing = 2.0f};
/// The side padding for the input plate stack view content (e.g. omnibox,
/// toolbar).
const NSDirectionalEdgeInsets kInputPlatePadding = {.leading = 8.0,
                                                    .trailing = 5.0};
/// The spacing added after the Lens and Voice buttons in compact mode.
const CGFloat kShortcutsTrailingPaddingCompact = 3.0f;
/// The padding of the toolbar.
///
/// Note: While padding is offset to visually align the clear button's visual
/// bounding box, all other UI elements maintain symmetrical centering.
const UIEdgeInsets kToolbarPadding = {.left = kInputPlatePadding.leading,
                                      .right = kInputPlatePadding.leading};
/// The padding of the carousel. Same as
/// `kInputPlateStackViewExpandedWithAttachmentsTopPadding` to keep symmetry.
const UIEdgeInsets kCarouselPadding = {
    .left = kInputPlateStackViewExpandedWithAttachmentsTopPadding,
    .right = kInputPlateStackViewExpandedWithAttachmentsTopPadding};

/// The font size for the AIM mode button title.
const CGFloat kAIMButtonFontSize = 14.0f;
/// The point size for the symbols in the AIM mode button.
const CGFloat kAIMButtonSymbolPointSize = 14;
/// The width of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonWidth = 24.0f;
/// The height of the buttons created with `createButtonWithImage:`.
const CGFloat kGenericButtonHeight = 32.0f;
/// The dimension of the send button.
const CGFloat kSendButtonDimension = 36.0f;
/// The dimension of the button stack view.
const CGFloat kButtonStackViewDimension = 36.0f;
/// Duration of a change in compact mode.
const CGFloat kCompactModeAnimationDuration = 0.1;
/// The opacity once the send button is disabled.
const CGFloat kSendButtonDisabledOpacity = 0.5;
/// The fade view width.
const CGFloat kFadeViewWidth = 30.0f;
/// The margin for the close mode button.
const CGFloat kCloseModeButtonMargin = 4;

/// The size of the close icon in the context indicator buttons.
const CGFloat kCloseIndicatorSize = 12.0f;

/// The index of the attachment section in the carousel.
const NSInteger kCarouselAttachmentSectionIndex = 0;

/// The image for the send button.
UIImage* SendButtonImage(BOOL highlighted, ComposeboxTheme* theme) {
  NSArray<UIColor*>* palette = @[
    [theme sendButtonForegroundColorHighlighted:highlighted],
    [theme sendButtonBackgroundColorHighlighted:highlighted]
  ];

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kSendButtonDimension
                          weight:UIImageSymbolWeightLight
                           scale:UIImageSymbolScaleMedium];

  return SymbolWithPalette(
      DefaultSymbolWithConfiguration(kRightArrowCircleFillSymbol, config),
      palette);
}

}  // namespace

@interface ComposeboxInputPlateViewController () <
    ComposeboxInputItemCellDelegate,
    UICollectionViewDelegate,
    UICollectionViewDelegateFlowLayout,
    UIDropInteractionDelegate,
    UITextViewDelegate>

/// Whether the AI mode is enabled.
@property(nonatomic, assign) BOOL AIModeEnabled;

/// The width constraint for the AIM button.
@property(nonatomic, strong) NSLayoutConstraint* aimButtonWidthConstraint;

/// Edit view contained in `_omniboxContainer`.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

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
  /// The view containing containing the plusButton, mic, send, etc.. in
  /// expanded mode.
  UIView* _toolbarView;
  /// An internal container that clips its bounds. This ensures extra AIM
  /// attachments do not overflow their container, and allows the input to flow
  /// closer to the edge of the box. Note that the external container still has
  /// shadows and glow effects attached; the use of a child view avoids
  /// interference with those.
  UIView* _inputPlateInternalContainerView;
  /// The button to toggle AI mode.
  UIButton* _aimButton;
  UIView* _aimButtonXIndicator;
  /// The button to toggle Image Generation mode.
  UIButton* _imageGenerationButton;
  /// The button to toggle Canvas mode.
  UIButton* _canvasButton;
  /// The button to toggle deep search mode.
  UIButton* _deepSearchButton;
  /// The button to attach the current tab.
  UIButton* _askAboutThisPageButton;
  /// The glow effect around the input plate container.
  UIView<GlowEffect>* _glowEffectView;
  /// The plus button.
  UIButton* _plusButton;
  /// The mic button for voice search.
  UIButton* _micButton;
  /// The camera scanner button, either to Lens or QR scanner.
  UIButton* _visualSearchButton;
  /// The fade view for the carousel's leading edge.
  UIView* _leadingCarouselFadeView;
  /// The fade view for the carousel's trailing edge.
  UIView* _trailingCarouselFadeView;
  __weak CAGradientLayer* _carouselLeadingGradientLayer;
  __weak CAGradientLayer* _carouselTrailingGradientLayer;
  /// The carousel container.
  UIView* _carouselContainer;
  /// Controls that should be visible.
  ComposeboxInputPlateControls _visibleControls;
  /// The current model choice.
  ComposeboxModelOption _modelOption;
  /// Attach current tab action state.
  BOOL _attachCurrentTabActionHidden;
  /// Attach tabs actions state.
  BOOL _attachTabActionsHidden;
  BOOL _attachTabActionsDisabled;
  /// Attach files action state.
  BOOL _attachFileActionsHidden;
  BOOL _attachFileActionsDisabled;
  /// Create image action state.
  BOOL _createImageActionsHidden;
  BOOL _createImageActionsDisabled;
  /// Canvas action state.
  BOOL _canvasActionsDisabled;
  BOOL _canvasActionsHidden;
  /// Deep search action state.
  BOOL _deepSearchActionsDisabled;
  BOOL _deepSearchActionsHidden;
  /// Camera action state.
  BOOL _cameraActionsDisabled;
  BOOL _cameraActionsHidden;
  /// Gallery action state.
  BOOL _galleryActionsDisabled;
  BOOL _galleryActionsHidden;
  /// The allowed and disabled models.
  std::unordered_set<ComposeboxModelOption> _allowedModels;
  std::unordered_set<ComposeboxModelOption> _disabledModels;
  /// Container for the omnibox.
  UIView* _omniboxContainer;

  // The theme of the composebox.
  ComposeboxTheme* _theme;

  // The favicon for the current tab.
  UIImage* _currentTabFavicon;

  // Constraints for the dynamic padding of the input plate stack view.
  NSLayoutConstraint* _topPaddingConstraint;
  NSLayoutConstraint* _bottomPaddingConstraint;

  /// Whether the image generation mode is enabled.
  BOOL _imageGenerationEnabled;

  /// Whether the canvas mode is enabled.
  BOOL _canvasEnabled;

  /// Whether the deep search is enabled.
  BOOL _deepSearchEnabled;

  /// Whether the model picker is allowed.
  BOOL _modelPickerAllowed;

  /// Whether items are being dragged within the input plate view.
  BOOL _dragSessionWithinInputPlate;

  /// The remaining capacity for attachments.
  NSUInteger _remainingAttachmentCapacity;

  /// The server side strings for the input plate elements.
  ComposeboxServerStrings* _serverStrings;
}

/// ComposeboxAnimationContext
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;
@synthesize keyboardHeight = _keyboardHeight;

- (instancetype)initWithTheme:(ComposeboxTheme*)theme {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    _omniboxContainer = [[UIView alloc] init];
    _theme = theme;
    _canvasActionsHidden = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // --- Bottom Input Area ---

  // Input plate container.
  [self setupInputPlateContainerView];
  AddSameConstraints(_inputPlateContainerView, self.view);

  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _micButton = [self createMicrophoneButton];
  _visualSearchButton = [self createVisualSearchButton];
  _plusButton = [self createPlusButton];
  _sendButton = [self createSendButton];
  _aimButton = [self createAIMButton];
  [self setupAIMButtonSizeConstraints];
  _imageGenerationButton = [self createImageGenerationButton];
  _canvasButton = [self createCanvasButton];
  _deepSearchButton = [self createDeepSearchButton];
  _askAboutThisPageButton = [self createAskAboutThisPageButton];
  [self updatePlusButtonItems];
  [self setupCarouselContainer];

  _inputPlateStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _omniboxContainer ]];
  _inputPlateStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_inputPlateInternalContainerView addSubview:_inputPlateStackView];

  _bottomPaddingConstraint = [_inputPlateStackView.bottomAnchor
      constraintEqualToAnchor:_inputPlateInternalContainerView.bottomAnchor
                     constant:-kInputPlateStackViewVerticalPadding];
  _topPaddingConstraint = [_inputPlateStackView.topAnchor
      constraintEqualToAnchor:_inputPlateInternalContainerView.topAnchor
                     constant:kInputPlateStackViewVerticalPadding];
  [NSLayoutConstraint
      activateConstraints:@[ _bottomPaddingConstraint, _topPaddingConstraint ]];

  AddSameConstraintsToSidesWithInsets(
      _inputPlateStackView, _inputPlateInternalContainerView,
      (LayoutSides::kLeading | LayoutSides::kTrailing),
      kInputPlateStackViewPadding);

  [self updateInputPlateStackViewAnimated:NO];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(userInterfaceStyleChanged)];

  [self.mutator requestUIRefresh];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update the gradient layer's frame.
  _carouselTrailingGradientLayer.frame = _trailingCarouselFadeView.bounds;
  _carouselLeadingGradientLayer.frame = _leadingCarouselFadeView.bounds;
  if (self.compact) {
    _inputPlateContainerView.layer.cornerRadius =
        _inputPlateContainerView.frame.size.height / 2;
    _inputPlateInternalContainerView.layer.cornerRadius =
        _inputPlateContainerView.layer.cornerRadius;
  }
  [self updateCarouselFade];
  [self updatePreferredContentSize];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Public

- (CGFloat)inputHeight {
  return _inputPlateContainerView.frame.size.height;
}

- (CGFloat)keyboardHeight {
  return _keyboardHeight;
}

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  _editView = editView;
  _editView.translatesAutoresizingMaskIntoConstraints = NO;
  _editView.minimumHeight = kOmniboxMinHeight;
  _editView.accessibilityIdentifier = kComposeboxAccessibilityIdentifier;
  [_omniboxContainer addSubview:_editView];
  [NSLayoutConstraint activateConstraints:@[
    [_editView.leadingAnchor
        constraintEqualToAnchor:_omniboxContainer.layoutMarginsGuide
                                    .leadingAnchor],
    [_editView.trailingAnchor
        constraintEqualToAnchor:_omniboxContainer.layoutMarginsGuide
                                    .trailingAnchor],
  ]];
  AddSameConstraintsToSides(_editView, _omniboxContainer,
                            LayoutSides::kTop | LayoutSides::kBottom);

  [self.mutator requestUIRefresh];
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
  [self updateInputPlateStackViewTopConstraint];
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
                    [weakSelf scrollToLast];
                    [weakSelf updatePreferredContentSize];
                  }];
}

- (void)updateState:(ComposeboxInputItemState)state
    forItemWithIdentifier:(const base::UnguessableToken&)identifier {
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxInputItem*>*
      currentSnapshot = _dataSource.snapshot;
  ComposeboxInputItem* itemToUpdate;
  for (ComposeboxInputItem* item in currentSnapshot.itemIdentifiers) {
    if (item.identifier == identifier) {
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
    [_editView setAllowsReturnKeyWithEmptyText:YES];
  } else {
    _sendButton.alpha = kSendButtonDisabledOpacity;
    _sendButton.enabled = NO;
    [_editView forceDisableReturnKey:YES];
    [_editView setAllowsReturnKeyWithEmptyText:NO];
  }
}

- (void)updateVisibleControls:(ComposeboxInputPlateControls)controls {
  using enum ComposeboxInputPlateControls;
  _visibleControls = controls;
  _plusButton.hidden = !(controls & kPlus);
  _micButton.hidden = !(controls & kVoice);
  [self updateCameraButton];

  [self updateToolbarVisibility];

  [self animateButton:_aimButton hidden:!(controls & kAIM)];
  [self animateButton:_askAboutThisPageButton
               hidden:!(controls & kAskAboutThisPage)];
  [self animateButton:_sendButton hidden:!(controls & kSend)];
  [self animateButton:_imageGenerationButton hidden:!(controls & kCreateImage)];
  [self animateButton:_canvasButton hidden:!(controls & kCanvas)];
  [self animateButton:_deepSearchButton hidden:!(controls & kDeepSearch)];
  [self animateLeadingImageHidden:!(controls & kLeadingImage)];

  [self updateInputPlateStackViewPadding];
}

- (void)animateReveal:(void (^)(void))animations {
  [UIView animateWithDuration:0.6 * kCompactModeAnimationDuration
                        delay:0.4 * kCompactModeAnimationDuration
                      options:UIViewAnimationCurveEaseInOut
                   animations:animations
                   completion:nil];
}

/// Whether `view` is hidden in `self.view` hierarchy either intrinsically or
/// indirectly by one of its superviews.
- (BOOL)isHiddenInHierarchy:(UIView*)view {
  UIView* controllingVisibility = view;
  do {
    if (controllingVisibility.hidden) {
      return YES;
    }

    controllingVisibility = controllingVisibility.superview;
  } while (controllingVisibility && controllingVisibility != self.view);

  return NO;
}

- (void)animateButton:(UIButton*)button hidden:(BOOL)hidden {
  BOOL alreadyHidden = button.hidden;
  button.hidden = hidden;
  // Only the appear sequence is animated.
  BOOL isAppearing = alreadyHidden && !hidden;
  if (!isAppearing) {
    return;
  }
  // If hidden indirectly by a superview, early return without animation.
  if ([self isHiddenInHierarchy:button]) {
    return;
  }
  button.alpha = 0;
  [self animateReveal:^{
    button.alpha = 1;
  }];
}

- (void)animateLeadingImageHidden:(BOOL)hidden {
  BOOL alreadyHidden = self.editView.leadingImageHidden;
  self.editView.leadingImageHidden = hidden;
  // Only the appear sequence is animated.
  BOOL isAppearing = alreadyHidden && !hidden;
  if (isAppearing) {
    [self.editView setLeadingImageAlpha:0];
    [self animateReveal:^{
      [self.editView setLeadingImageAlpha:1];
    }];
  }
}

- (void)setCompact:(BOOL)compact {
  if (_compact == compact) {
    return;
  }
  _compact = compact;

  if (!self.viewLoaded) {
    return;
  }

  [self updateInputPlateStackViewAnimated:YES];
}

- (void)setAIModeEnabled:(BOOL)enabled {
  if (_AIModeEnabled == enabled) {
    return;
  }
  _AIModeEnabled = enabled;
  [self updatePlaceholderText];
  [self updateAIMButtonAppearance];
  [self updatePlusButtonItems];
  [self triggerGlowEffect];
}

- (void)setImageGenerationEnabled:(BOOL)enabled {
  if (_imageGenerationEnabled == enabled) {
    return;
  }
  _imageGenerationEnabled = enabled;
  [self updatePlaceholderText];
  [self updatePlusButtonItems];
  [self triggerGlowEffect];
}

// Enables usage of canvas mode.
- (void)setCanvasEnabled:(BOOL)enabled {
  if (_canvasEnabled == enabled) {
    return;
  }
  _canvasEnabled = enabled;
  [self updatePlaceholderText];
  [self updatePlusButtonItems];
  [self triggerGlowEffect];
}

- (void)setDeepSearchEnabled:(BOOL)enabled {
  if (_deepSearchEnabled == enabled) {
    return;
  }
  _deepSearchEnabled = enabled;
  [self updatePlaceholderText];
  [self updatePlusButtonItems];
  [self triggerGlowEffect];
}

- (void)allowModelPicker:(BOOL)allowed {
  if (_modelPickerAllowed == allowed) {
    return;
  }
  _modelPickerAllowed = allowed;
  [self updatePlaceholderText];
  [self updatePlusButtonItems];
}

- (void)setCurrentTabFavicon:(UIImage*)favicon {
  _currentTabFavicon =
      favicon ? ImageWithCornerRadius(favicon, kAttachCurrentTabIconRadius)
              : nil;
  [self updatePlusButtonItems];
}

- (void)hideAttachCurrentTabAction:(BOOL)hidden {
  if (_attachCurrentTabActionHidden == hidden) {
    return;
  }
  _attachCurrentTabActionHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)hideAttachTabActions:(BOOL)hidden {
  if (_attachTabActionsHidden == hidden) {
    return;
  }
  _attachTabActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableAttachTabActions:(BOOL)disabled {
  if (_attachTabActionsDisabled == disabled) {
    return;
  }
  _attachTabActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)hideAttachFileActions:(BOOL)hidden {
  if (_attachFileActionsHidden == hidden) {
    return;
  }
  _attachFileActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableAttachFileActions:(BOOL)disabled {
  if (_attachFileActionsDisabled == disabled) {
    return;
  }
  _attachFileActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)hideCreateImageActions:(BOOL)hidden {
  if (_createImageActionsHidden == hidden) {
    return;
  }
  _createImageActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableCreateImageActions:(BOOL)disabled {
  if (_createImageActionsDisabled == disabled) {
    return;
  }
  _createImageActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

// Hides the canvas actions in the plus menu.
- (void)hideCanvasActions:(BOOL)hidden {
  if (_canvasActionsHidden == hidden) {
    return;
  }
  _canvasActionsHidden = hidden;
  [self updatePlusButtonItems];
}

// Hides the deep search actions in the plus menu.
- (void)hideDeepSearchActions:(BOOL)hidden {
  if (_deepSearchActionsHidden == hidden) {
    return;
  }
  _deepSearchActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableDeepSearchActions:(BOOL)disabled {
  if (_deepSearchActionsDisabled == disabled) {
    return;
  }
  _deepSearchActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)hideCameraActions:(BOOL)hidden {
  if (_cameraActionsHidden == hidden) {
    return;
  }
  _cameraActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableCanvasActions:(BOOL)disabled {
  if (_canvasActionsDisabled == disabled) {
    return;
  }
  _canvasActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)disableCameraActions:(BOOL)disabled {
  if (_cameraActionsDisabled == disabled) {
    return;
  }
  _cameraActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)hideGalleryActions:(BOOL)hidden {
  if (_galleryActionsHidden == hidden) {
    return;
  }
  _galleryActionsHidden = hidden;
  [self updatePlusButtonItems];
}

- (void)disableGalleryActions:(BOOL)disabled {
  if (_galleryActionsDisabled == disabled) {
    return;
  }
  _galleryActionsDisabled = disabled;
  [self updatePlusButtonItems];
}

- (void)setAllowedModels:
    (std::unordered_set<ComposeboxModelOption>)allowedModels {
  if (_allowedModels == allowedModels) {
    return;
  }
  _allowedModels = allowedModels;
  [self updatePlusButtonItems];
}

- (void)setDisabledModels:
    (std::unordered_set<ComposeboxModelOption>)disabledModels {
  if (_disabledModels == disabledModels) {
    return;
  }
  _disabledModels = disabledModels;
  [self updatePlusButtonItems];
}

- (void)setServerStrings:(ComposeboxServerStrings*)serverStrings {
  _serverStrings = serverStrings;
  [self updatePlusButtonItems];
  [self updatePlaceholderText];
}

- (void)setRemainingAttachmentCapacity:(NSUInteger)capacity {
  _remainingAttachmentCapacity = capacity;
}

- (void)setModelOption:(ComposeboxModelOption)modelOption {
  _modelOption = modelOption;
  [self updatePlusButtonItems];
  [self updateCreateImageTitle];
}

- (void)updatePreferredContentSizeForNewTextFieldHeight {
  // Trigger -viewDidLayoutSubviews that will call -updatePreferredContentSize.
  [_omniboxContainer layoutIfNeeded];
}

#pragma mark - Actions

- (void)galleryButtonTapped {
  [self.delegate composeboxViewControllerDidTapGalleryButton:self];
}

- (void)cameraButtonTapped {
  [self.delegate composeboxViewControllerDidTapCameraButton:self];
}

- (void)aimButtonTapped {
  [self.delegate
      composeboxViewControllerDidTapAIMButton:self
                             activationSource:AiModeActivationSource::
                                                  kDedicatedButton];
}

- (void)imageGenerationButtonTapped {
  [self.delegate composeboxViewControllerDidTapImageGenerationButton:self];
}

// Called when the canvas button in the input plate is tapped.
- (void)canvasButtonTapped {
  [self.delegate composeboxViewControllerDidTapCanvasButton:self];
}

// Called when the deep search button in the input plate is tapped.
- (void)deepSearchButtonTapped {
  [self.delegate composeboxViewControllerDidTapDeepSearchButton:self];
}

// Called when the Ask about this page button in the input plate is tapped.
- (void)askAboutThisPageButtonTapped {
  [self.mutator attachCurrentTabContent];
}

- (void)plusButtonTouchDown {
  [self.delegate composeboxViewControllerMayShowGalleryPicker:self];
}

- (void)micButtonTapped {
  [self.delegate composeboxViewController:self didTapMicButton:_micButton];
}

- (void)visualSearchButtonTapped {
  using enum ComposeboxInputPlateControls;
  if ((_visibleControls & kLens) != kNone) {
    [self.delegate composeboxViewController:self
                           didTapLensButton:_visualSearchButton];
  } else if ((_visibleControls & kQRScanner) != kNone) {
    [self.delegate composeboxViewController:self
                      didTapQRScannerButton:_visualSearchButton];
  }
}

- (void)sendButtonTapped {
  [self.delegate composeboxViewController:self didTapSendButton:_sendButton];
}

#pragma mark - UIDropInteractionDelegate

- (BOOL)dropInteraction:(UIDropInteraction*)interaction
       canHandleSession:(id<UIDropSession>)session {
  _dragSessionWithinInputPlate = YES;
  return YES;
}

- (UIDropProposal*)dropInteraction:(UIDropInteraction*)interaction
                  sessionDidUpdate:(id<UIDropSession>)session {
  return [[UIDropProposal alloc]
      initWithDropOperation:[self isDropAllowed:session]
                                ? UIDropOperationCopy
                                : UIDropOperationForbidden];
}

- (void)dropInteraction:(UIDropInteraction*)interaction
            performDrop:(id<UIDropSession>)session {
  [self performDrop:session];
}

- (void)dropInteraction:(UIDropInteraction*)interaction
         sessionDidExit:(id<UIDropSession>)session {
  _dragSessionWithinInputPlate = NO;
}

- (void)dropInteraction:(UIDropInteraction*)interaction
          sessionDidEnd:(id<UIDropSession>)session {
  [self dropSessionDidEnd:session];
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

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if (![cell isKindOfClass:[ComposeboxInputItemCell class]]) {
    return;
  }

  // If the evicted cell’s associated input item is no longer in the data
  // source, it was likely removed by the user.
  // Proactively prepare the cell for reuse now to help alleviate memory
  // pressure.
  ComposeboxInputItemCell* composeboxCell = (ComposeboxInputItemCell*)cell;
  if (ComposeboxInputItem* associatedItem = composeboxCell.associatedItem) {
    for (ComposeboxInputItem* item in _dataSource.snapshot.itemIdentifiers) {
      if (item.identifier == associatedItem.identifier) {
        return;
      }
    }
  }

  [composeboxCell prepareForReuse];
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateCarouselFade];
}

/// Updates carousel fading gradients colors.
- (void)updateCarouselGradientAppearance {
  // Update the gradient layer's color.
  UIColor* transparentColor =
      [_theme.inputPlateBackgroundColor colorWithAlphaComponent:0.0];
  UIColor* solidColor = _theme.inputPlateBackgroundColor;

  _carouselLeadingGradientLayer.colors =
      @[ (id)solidColor.CGColor, (id)transparentColor.CGColor ];
  _carouselTrailingGradientLayer.colors =
      @[ (id)transparentColor.CGColor, (id)solidColor.CGColor ];

  [_carouselLeadingGradientLayer setNeedsDisplay];
  [_carouselTrailingGradientLayer setNeedsDisplay];
}

/// Returns a gradient layer for the carousel.
- (CAGradientLayer*)createCarouselGradientLayer {
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  return gradientLayer;
}

- (UICollectionViewDiffableDataSource<NSString*, ComposeboxInputItem*>*)
    createDataSource {
  __weak ComposeboxTheme* theme = _theme;
  __weak __typeof(self) weakSelf = self;
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
                  cell.delegate = weakSelf;
                  return cell;
                }];
}

#pragma mark - Private helpers

/// Handles keyboard appearance notifications to adjust layout.
- (void)keyboardWillShow:(NSNotification*)notification {
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  _keyboardHeight = keyboardFrame.size.height;
}

/// Handles keyboard disappearance notifications to reset layout.
- (void)keyboardWillHide:(NSNotification*)notification {
  _keyboardHeight = 0;
}

/// Notifies the delegate to show the tab attachment UI.
- (void)handleAttachTabs {
  [self.delegate composeboxViewControllerDidTapAttachTabsButton:self];
}

/// Notifies the delegate to handle AIM tapped from the tool menu.
- (void)handleAIMTappedFromToolMenu {
  [self.delegate composeboxViewControllerDidTapAIMButton:self
                                        activationSource:
                                            AiModeActivationSource::kToolMenu];
}

/// Notifies the delegate to handle canvas button tapped from the tool menu.
- (void)handleImageGenTappedFromToolMenu {
  [self.delegate composeboxViewControllerDidTapImageGenerationButton:self];
}

/// Notifies the delegate to handle canvas tapped from the tool menu.
- (void)handleCanvasTappedFromToolMenu {
  [self.delegate composeboxViewControllerDidTapCanvasButton:self];
}

/// Notifies the delegate to handle deep search tapped from the tool menu.
- (void)handleDeepSearchTappedFromToolMenu {
  [self.delegate composeboxViewControllerDidTapDeepSearchButton:self];
}

/// Notifies the mutator to handle the selection of a new model option.
- (void)handleModelChangeFromToolsMenuWithOption:
    (ComposeboxModelOption)modelOption {
  [self.mutator setModelOption:modelOption explicitUserAction:YES];
}

/// Updates the visibility of the leading/trailing fade views for the carousel.
- (void)updateCarouselFade {
  CGFloat contentOffsetX = _carouselView.contentOffset.x;
  CGFloat contentWidth = _carouselView.contentSize.width;
  CGFloat boundsWidth = _carouselView.bounds.size.width;

  _leadingCarouselFadeView.hidden = contentOffsetX <= 0;
  _trailingCarouselFadeView.hidden =
      contentOffsetX + boundsWidth >= contentWidth;
}

/// Scrolls the last item in `_carouselView` into view.
- (void)scrollToLast {
  if (!_carouselView) {
    return;
  }
  // Ensure the content width actually overflows. If not, no-op.
  CGFloat contentOffsetX = _carouselView.contentOffset.x;
  CGFloat contentWidth = _carouselView.contentSize.width;
  CGFloat boundsWidth = _carouselView.bounds.size.width;
  if (contentOffsetX + boundsWidth >= contentWidth) {
    return;
  }

  BOOL attachmentSectionIsPresent = [_carouselView numberOfSections] != 0;
  if (!attachmentSectionIsPresent) {
    return;
  }
  NSInteger lastItemIndex =
      [_carouselView numberOfItemsInSection:kCarouselAttachmentSectionIndex] -
      1;
  if (lastItemIndex < 0) {
    return;
  }
  NSIndexPath* lastItemIndexPath =
      [NSIndexPath indexPathForItem:lastItemIndex
                          inSection:kCarouselAttachmentSectionIndex];
  [_carouselView
      scrollToItemAtIndexPath:lastItemIndexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredHorizontally
                     animated:YES];
}

/// Updates the input plate stack view top padding.
- (void)updateInputPlateStackViewTopConstraint {
  if (_carouselContainer.hidden) {
    _topPaddingConstraint.constant = kInputPlateStackViewVerticalPadding;
  } else {
    _topPaddingConstraint.constant =
        kInputPlateStackViewExpandedWithAttachmentsTopPadding;
  }
}

/// Initiates the glow animation around the input plate.
- (void)triggerGlowEffect {
  if (!_glowEffectView) {
    return;
  }

  if (_AIModeEnabled || _imageGenerationEnabled || _canvasEnabled) {
    // When turning on, ensure the glow is started. The view's state machine
    // will prevent it from restarting if it's already active.
    [_glowEffectView startGlow];
  }
}

/// Responds to changes in the user interface style (e.g.: dark/light mode).
- (void)userInterfaceStyleChanged {
  [self updateAIMButtonAppearance];
  [self updateDepthShadowAppearance];
  [self updateCarouselGradientAppearance];
}

/// Adjusts the shadow of the input plate based on UI style and theme.
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

  UIButtonConfiguration* config = _aimButton.configuration;
  NSDirectionalEdgeInsets insets = kModeIndicatorButtonInsets;
  self.aimButtonWidthConstraint.constant = kAIMButtonBaseWidth;
  if (_AIModeEnabled) {
    insets.trailing += kXButtonWidthInButton;
    self.aimButtonWidthConstraint.constant += kXButtonWidthInButton;
  }
  config.contentInsets = insets;
  config.background.backgroundColor =
      [_theme aimButtonBackgroundColorWithAIMEnabled:_AIModeEnabled];
  config.baseForegroundColor =
      [_theme aimButtonTextColorWithAIMEnabled:_AIModeEnabled];
  config.background.strokeWidth = _AIModeEnabled ? 0 : 1;
  config.background.strokeColor =
      [_theme aimButtonBorderColorWithAIMEnabled:_AIModeEnabled];

  _aimButton.accessibilityLabel = l10n_util::GetNSString(
      _AIModeEnabled
          ? IDS_IOS_COMPOSEBOX_AIM_BUTTON_DISABLE_ACTION_ACCESSIBILITY_LABEL
          : IDS_IOS_COMPOSEBOX_AIM_BUTTON_ENABLE_ACTION_ACCESSIBILITY_LABEL);

  _aimButton.configuration = config;

  // Setup the X mark only after the config was aplied, otherwise the
  // constraints applied relative to the title label will be wrong for iOS 18.
  [_aimButtonXIndicator removeFromSuperview];
  _aimButtonXIndicator = nil;

  if (_AIModeEnabled) {
    _aimButtonXIndicator = [self setupXMarkInButton:_aimButton];
  }
}

/// Updates the placeholder text based on the current operating mode of the
/// composebox.
- (void)updatePlaceholderText {
  using enum InputPlateString;

  InputPlateString element;
  if (_AIModeEnabled) {
    element = kAIM;
  } else if (_imageGenerationEnabled) {
    element = kImageGeneration;
  } else if (_canvasEnabled) {
    element = kCanvas;
  } else if (_deepSearchEnabled) {
    element = kDeepSearch;
  } else {
    [_editView setCustomPlaceholderText:nil];
    return;
  }
  [_editView
      setCustomPlaceholderText:[self titleFor:element
                                         type:InputPlateStringType::kHintText]];
}

/// Adds and constraints the 'X' mark indicator to the given button.
- (UIView*)setupXMarkInButton:(UIButton*)button {
  UIImageView* xMarkImageView = [[UIImageView alloc] init];
  xMarkImageView.translatesAutoresizingMaskIntoConstraints = NO;
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseIndicatorSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  xMarkImageView.image =
      DefaultSymbolWithConfiguration(kXMarkSymbol, configuration);
  // The parent button view is the relevant element.
  xMarkImageView.isAccessibilityElement = NO;
  [button addSubview:xMarkImageView];

  [NSLayoutConstraint activateConstraints:@[
    [button.titleLabel.trailingAnchor
        constraintEqualToAnchor:xMarkImageView.leadingAnchor
                       constant:-kCloseModeButtonMargin],
    [button.titleLabel.centerYAnchor
        constraintEqualToAnchor:xMarkImageView.centerYAnchor],
  ]];

  return xMarkImageView;
}

/// Creates an extended touch target button with the given `image`.
- (UIButton*)createButtonWithImage:(UIImage*)image {
  UIButton* button =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  NSLayoutConstraint* widthConstraint =
      [button.widthAnchor constraintEqualToConstant:kGenericButtonWidth];
  widthConstraint.active = YES;
  widthConstraint.priority = UILayoutPriorityRequired - 1;
  NSLayoutConstraint* heightConstraint = [button.heightAnchor
      constraintGreaterThanOrEqualToConstant:kGenericButtonHeight];
  heightConstraint.active = YES;
  heightConstraint.priority = UILayoutPriorityRequired - 1;

  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  return button;
}

/// Creates the AI Mode button.
- (UIButton*)createAIMButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.configurationUpdateHandler =
      [self configurationUpdateHandlerForModeIndicator];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(aimButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityTraits = UIAccessibilityTraitButton;
  button.accessibilityIdentifier = kComposeboxAIMButtonAccessibilityIdentifier;

  UIImage* icon = CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                            kAIMButtonSymbolPointSize);

  NSString* title = [self titleFor:InputPlateString::kAIM
                              type:InputPlateStringType::kChipLabel];
  button.configuration = [self modeIndicatorButtonConfigWithTitle:title
                                                            image:icon];

  return button;
}

- (void)setupAIMButtonSizeConstraints {
  self.aimButtonWidthConstraint =
      [_aimButton.widthAnchor constraintEqualToConstant:kAIMButtonBaseWidth];

  [NSLayoutConstraint activateConstraints:@[
    [_aimButton.heightAnchor constraintEqualToConstant:kAIMButtonHeight],
    self.aimButtonWidthConstraint
  ]];
}

/// Creates the plus button that contains the menu.
- (UIButton*)createPlusButton {
  UIButton* plusButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [plusButton
      setImage:DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize)
      forState:UIControlStateNormal];
  plusButton.translatesAutoresizingMaskIntoConstraints = NO;
  plusButton.imageView.contentMode = UIViewContentModeScaleAspectFit;
  plusButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  plusButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL);
  plusButton.accessibilityIdentifier =
      kComposeboxPlusButtonAccessibilityIdentifier;

  [NSLayoutConstraint activateConstraints:@[
    [plusButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kAIMButtonHeight],
    [plusButton.widthAnchor constraintEqualToConstant:kAIMButtonHeight],
  ]];

  [plusButton addTarget:self
                 action:@selector(plusButtonTouchDown)
       forControlEvents:UIControlEventTouchDown];
  plusButton.showsMenuAsPrimaryAction = YES;

  return plusButton;
}

/// Returns the send button.
- (UIButton*)createSendButton {
  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfig.image = SendButtonImage(/*highlighted=*/NO, _theme);
  buttonConfig.contentInsets = NSDirectionalEdgeInsetsZero;

  UIButton* sendButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  sendButton.configuration = buttonConfig;

  __weak __typeof(self) weakSelf = self;
  sendButton.configurationUpdateHandler = ^(UIButton* button) {
    [weakSelf sendButtonDidUpdateConfiguration];
  };
  sendButton.accessibilityIdentifier =
      kComposeboxSendButtonAccessibilityIdentifier;
  sendButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SEARCH);

  [sendButton addTarget:self
                 action:@selector(sendButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  AddSizeConstraints(sendButton,
                     CGSizeMake(kSendButtonDimension, kSendButtonDimension));
  return sendButton;
}

// Called when the configuration of the send button is updated.
- (void)sendButtonDidUpdateConfiguration {
  UIButtonConfiguration* updatedConfig = _sendButton.configuration;
  BOOL isHighlighted = _sendButton.state == UIControlStateHighlighted;
  updatedConfig.image = SendButtonImage(isHighlighted, _theme);
  _sendButton.configuration = updatedConfig;
  CGFloat scale = isHighlighted ? 0.95 : 1.0;
  __weak UIButton* weakSendButton = _sendButton;
  [UIView animateWithDuration:0.1
                   animations:^{
                     weakSendButton.transform =
                         CGAffineTransformMakeScale(scale, scale);
                   }];
}

/// Returns the microphone button.
- (UIButton*)createMicrophoneButton {
  UIButton* micButton =
      [self createButtonWithImage:CustomSymbolWithPointSize(
                                      kVoiceSymbol, kSymbolActionPointSize)];
  micButton.imageView.contentMode = UIViewContentModeScaleAspectFit;
  micButton.accessibilityIdentifier =
      kComposeboxMicButtonAccessibilityIdentifier;
  micButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_VOICE_SEARCH);

  [micButton addTarget:self
                action:@selector(micButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  return micButton;
}

/// Returns the visual search button (Lens or QR Code).
- (UIButton*)createVisualSearchButton {
  UIButton* visualSearchButton = [self createButtonWithImage:nil];
  visualSearchButton.imageView.contentMode = UIViewContentModeScaleAspectFit;

  [visualSearchButton addTarget:self
                         action:@selector(visualSearchButtonTapped)
               forControlEvents:UIControlEventTouchUpInside];

  return visualSearchButton;
}

/// Updates the toolbar visiblity depending on state of the buttons that should
/// be visible.
- (void)updateToolbarVisibility {
  using enum ComposeboxInputPlateControls;
  ComposeboxInputPlateControls requiredControlsForVisibility =
      (kPlus | kVoice | kLens | kSend | kAskAboutThisPage);
  _toolbarView.hidden = !(_visibleControls & requiredControlsForVisibility);

  if (!self.compact) {
    _bottomPaddingConstraint.constant =
        _toolbarView.hidden ? 0 : -kInputPlateStackViewExpandedBottomPadding;
  }
}

- (void)updateCameraButton {
  using enum ComposeboxInputPlateControls;
  if ((_visibleControls & kLens) != kNone) {
    _visualSearchButton.hidden = NO;
    [_visualSearchButton setImage:CustomSymbolWithPointSize(
                                      kCameraLensSymbol, kSymbolActionPointSize)
                         forState:UIControlStateNormal];
    _visualSearchButton.accessibilityIdentifier =
        kComposeboxLensButtonAccessibilityIdentifier;
    _visualSearchButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  } else if ((_visibleControls & kQRScanner) != kNone) {
    _visualSearchButton.hidden = NO;
    [_visualSearchButton
        setImage:DefaultSymbolWithPointSize(kQRCodeFinderActionSymbol,
                                            kSymbolActionPointSize)
        forState:UIControlStateNormal];

    _visualSearchButton.accessibilityIdentifier =
        kComposeboxQRCodeButtonAccessibilityIdentifier;
    _visualSearchButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH);
  } else {
    _visualSearchButton.hidden = YES;
  }
}

/// Creates and returns the toolbar view containing action buttons.
- (UIView*)createToolbarView {
  [self updateAIMButtonAppearance];

  // Horizontal stack view for buttons
  UIView* spacerView = [[UIView alloc] init];
  [spacerView setContentHuggingPriority:UILayoutPriorityFittingSizeLevel
                                forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _plusButton, _aimButton, _imageGenerationButton, _canvasButton,
        _deepSearchButton, _askAboutThisPageButton, spacerView, _sendButton,
        _micButton, _visualSearchButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [buttonsStackView setCustomSpacing:kShortcutsSpacing afterView:_micButton];
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = kButtonsStackViewSpacing;
  buttonsStackView.alignment = UIStackViewAlignmentFill;
  [NSLayoutConstraint activateConstraints:@[
    [buttonsStackView.heightAnchor
        constraintEqualToConstant:kButtonStackViewDimension]
  ]];
  buttonsStackView.layoutMarginsRelativeArrangement = YES;
  buttonsStackView.layoutMargins = kToolbarPadding;
  return buttonsStackView;
}

/// Configures the menu items for the plus (+) button.
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
  galleryAction.accessibilityIdentifier =
      kComposeboxGalleryActionAccessibilityIdentifier;
  UIAction* cameraAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
                image:DefaultSymbolWithPointSize(kSystemCameraSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    composeboxViewControllerDidTapCameraButton:weakSelf];
              }];
  cameraAction.accessibilityIdentifier =
      kComposeboxCameraActionAccessibilityIdentifier;

  UIAction* fileAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
                image:DefaultSymbolWithPointSize(kDocSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate
                    composeboxViewControllerDidTapFileButton:weakSelf];
              }];
  fileAction.accessibilityIdentifier =
      kComposeboxAttachFileActionAccessibilityIdentifier;

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
  attachCurrentTabAction.accessibilityIdentifier =
      kComposeboxAttachCurrentTabActionAccessibilityIdentifier;

  UIAction* selectTabsAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
                image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleAttachTabs];
              }];
  selectTabsAction.accessibilityIdentifier =
      kComposeboxSelectTabsActionAccessibilityIdentifier;

  UIAction* aimAction = [UIAction
      actionWithTitle:[self titleFor:InputPlateString::kAIM
                                type:InputPlateStringType::kMenuLabel]
                image:CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                                kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleAIMTappedFromToolMenu];
              }];
  aimAction.accessibilityIdentifier =
      kComposeboxAIMActionAccessibilityIdentifier;

  if (self.AIModeEnabled) {
    [aimAction setState:UIMenuElementStateOn];
  }

  UIAction* createImageAction = [UIAction
      actionWithTitle:[self titleFor:InputPlateString::kImageGeneration
                                type:InputPlateStringType::kMenuLabel]
                image:[self bananaIcon]
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleImageGenTappedFromToolMenu];
              }];
  createImageAction.accessibilityIdentifier =
      kComposeboxImageGenerationActionAccessibilityIdentifier;

  if (_imageGenerationEnabled) {
    [createImageAction setState:UIMenuElementStateOn];
  }

  UIMenuElementAttributes attachTabAttributes = 0;
  if (_attachTabActionsHidden) {
    attachTabAttributes |= UIMenuElementAttributesHidden;
  }

  if (_attachTabActionsDisabled) {
    attachTabAttributes |= UIMenuElementAttributesDisabled;
  }
  selectTabsAction.attributes = attachTabAttributes;

  UIMenuElementAttributes attachCurrentTabAttributes = attachTabAttributes;
  if (_attachCurrentTabActionHidden) {
    attachCurrentTabAttributes |= UIMenuElementAttributesHidden;
  }
  attachCurrentTabAction.attributes = attachCurrentTabAttributes;

  UIMenuElementAttributes attachFileAttributes = 0;
  if (_attachFileActionsHidden) {
    attachFileAttributes |= UIMenuElementAttributesHidden;
  }
  if (_attachFileActionsDisabled) {
    attachFileAttributes |= UIMenuElementAttributesDisabled;
  }
  fileAction.attributes = attachFileAttributes;

  UIMenuElementAttributes createImageAttributes = 0;
  if (_createImageActionsHidden) {
    createImageAttributes |= UIMenuElementAttributesHidden;
  }
  if (_createImageActionsDisabled) {
    createImageAttributes |= UIMenuElementAttributesDisabled;
  }
  createImageAction.attributes = createImageAttributes;

  UIMenuElementAttributes galleryAttributes = 0;
  if (_galleryActionsHidden) {
    galleryAttributes |= UIMenuElementAttributesHidden;
  }
  if (_galleryActionsDisabled) {
    galleryAttributes |= UIMenuElementAttributesDisabled;
  }
  galleryAction.attributes = galleryAttributes;

  UIMenuElementAttributes cameraAttributes = 0;
  if (_cameraActionsHidden) {
    cameraAttributes |= UIMenuElementAttributesHidden;
  }
  if (_cameraActionsDisabled) {
    cameraAttributes |= UIMenuElementAttributesDisabled;
  }
  cameraAction.attributes = cameraAttributes;

  UIMenu* attachmentMenu =
      [UIMenu menuWithTitle:@""
                      image:nil
                 identifier:nil
                    options:UIMenuOptionsDisplayInline
                   children:@[
                     attachCurrentTabAction, selectTabsAction, cameraAction,
                     galleryAction, fileAction
                   ]];

  UIAction* canvasAction =
      [UIAction actionWithTitle:[self titleFor:InputPlateString::kCanvas
                                          type:InputPlateStringType::kMenuLabel]
                          image:CustomSymbolWithPointSize(
                                    kDocumentBadgeSpark, kSymbolActionPointSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf handleCanvasTappedFromToolMenu];
                        }];
  UIMenuElementAttributes canvasAttributes = 0;
  if (_canvasActionsHidden) {
    canvasAttributes |= UIMenuElementAttributesHidden;
  }
  if (_canvasActionsDisabled) {
    canvasAttributes |= UIMenuElementAttributesDisabled;
  }
  canvasAction.attributes = canvasAttributes;

  if (_canvasEnabled) {
    [canvasAction setState:UIMenuElementStateOn];
  }

  NSString* deepSearchTitle = [self titleFor:InputPlateString::kDeepSearch
                                        type:InputPlateStringType::kMenuLabel];
  UIAction* deepSearchAction =
      [UIAction actionWithTitle:deepSearchTitle
                          image:CustomSymbolWithPointSize(
                                    kDeepSearchSymbol, kSymbolActionPointSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf handleDeepSearchTappedFromToolMenu];
                        }];
  UIMenuElementAttributes deepSearchAttributes = 0;
  if (_deepSearchActionsHidden) {
    deepSearchAttributes |= UIMenuElementAttributesHidden;
  }
  if (_deepSearchActionsDisabled) {
    deepSearchAttributes |= UIMenuElementAttributesDisabled;
  }
  deepSearchAction.attributes = deepSearchAttributes;

  if (_deepSearchEnabled) {
    [deepSearchAction setState:UIMenuElementStateOn];
  }

  NSString* toolsSectionTitle =
      [self titleFor:InputPlateString::kToolsSection
                type:InputPlateStringType::kMenuLabel];
  UIMenu* modeMenu = [UIMenu
      menuWithTitle:toolsSectionTitle
              image:nil
         identifier:nil
            options:UIMenuOptionsDisplayInline
           children:@[
             aimAction, createImageAction, deepSearchAction, canvasAction
           ]];

  NSMutableArray<UIMenuElement*>* sections =
      [[NSMutableArray alloc] initWithArray:@[ attachmentMenu, modeMenu ]];
  if (_modelPickerAllowed) {
    CHECK(ShowComposeboxAdditionalAdvancedTools());

    // Note: When possible, this is meant to be replaced by 'Auto'.
    UIAction* regularModelOption = [UIAction
        actionWithTitle:[self titleFor:InputPlateString::kRegularModel
                                  type:InputPlateStringType::kMenuLabel]
                  image:DefaultSymbolWithPointSize(kBoltSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf handleModelChangeFromToolsMenuWithOption:
                                ComposeboxModelOption::kRegular];
                }];

    if (!_allowedModels.contains(ComposeboxModelOption::kRegular) ||
        _allowedModels.contains(ComposeboxModelOption::kAuto)) {
      regularModelOption.attributes |= UIMenuElementAttributesHidden;
    }
    if (_disabledModels.contains(ComposeboxModelOption::kRegular)) {
      regularModelOption.attributes |= UIMenuElementAttributesDisabled;
    }
    if (_modelOption == ComposeboxModelOption::kRegular) {
      [regularModelOption setState:UIMenuElementStateOn];
    }

    NSString* autoModelTitle = [self titleFor:InputPlateString::kAutoModel
                                         type:InputPlateStringType::kMenuLabel];
    UIAction* autoModelOption = [UIAction
        actionWithTitle:autoModelTitle
                  image:DefaultSymbolWithPointSize(kSyncEnabledSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf handleModelChangeFromToolsMenuWithOption:
                                ComposeboxModelOption::kAuto];
                }];

    if (!_allowedModels.contains(ComposeboxModelOption::kAuto)) {
      autoModelOption.attributes |= UIMenuElementAttributesHidden;
    }
    if (_disabledModels.contains(ComposeboxModelOption::kAuto)) {
      autoModelOption.attributes |= UIMenuElementAttributesDisabled;
    }
    if (_modelOption == ComposeboxModelOption::kAuto) {
      [autoModelOption setState:UIMenuElementStateOn];
    }

    UIAction* thinkingModelOption = [UIAction
        actionWithTitle:[self titleFor:InputPlateString::kThinkingModel
                                  type:InputPlateStringType::kMenuLabel]
                  image:DefaultSymbolWithPointSize(kClockSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf handleModelChangeFromToolsMenuWithOption:
                                ComposeboxModelOption::kThinking];
                }];

    if (!_allowedModels.contains(ComposeboxModelOption::kThinking)) {
      thinkingModelOption.attributes |= UIMenuElementAttributesHidden;
    }
    if (_disabledModels.contains(ComposeboxModelOption::kThinking)) {
      thinkingModelOption.attributes |= UIMenuElementAttributesDisabled;
    }
    if (_modelOption == ComposeboxModelOption::kThinking) {
      [thinkingModelOption setState:UIMenuElementStateOn];
    }

    NSString* modelPickerTitle =
        [self titleFor:InputPlateString::kModelsSection
                  type:InputPlateStringType::kMenuLabel];
    UIMenu* modelPickerMenu =
        [UIMenu menuWithTitle:modelPickerTitle
                        image:nil
                   identifier:nil
                      options:UIMenuOptionsDisplayInline
                     children:@[
                       regularModelOption, autoModelOption, thinkingModelOption
                     ]];

    [sections addObject:modelPickerMenu];
  }

  _plusButton.menu = [UIMenu
      menuWithTitle:IsComposeboxMenuTitleEnabled()
                        ? l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_TITLE)
                        : @""
           children:sections];
  _plusButton.preferredMenuElementOrder =
      UIContextMenuConfigurationElementOrderFixed;
}

/// Initializes and configures the collection view for the attachment carousel.
- (void)setupCarouselContainer {
  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.accessibilityIdentifier =
      kComposeboxCarouselAccessibilityIdentifier;
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[ComposeboxInputItemCell class]
      forCellWithReuseIdentifier:kItemCellReuseIdentifier];
  _dataSource = [self createDataSource];
  _carouselView.dataSource = _dataSource;
  _carouselView.delegate = self;
  [_carouselView.heightAnchor constraintEqualToConstant:kCarouselHeight]
      .active = YES;
  // The outer view has minimal padding to allow the carousel space for multiple
  // attachments when they overflow. This ensures that there's still some
  // padding when the carousel is scrolled to either end.
  _carouselView.contentInset = kCarouselPadding;
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

  _carouselLeadingGradientLayer = [self createCarouselGradientLayer];
  _carouselTrailingGradientLayer = [self createCarouselGradientLayer];

  [_trailingCarouselFadeView.layer insertSublayer:_carouselTrailingGradientLayer
                                          atIndex:0];
  [_leadingCarouselFadeView.layer insertSublayer:_carouselLeadingGradientLayer
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
  [self updateCarouselGradientAppearance];
}

/// Sets up the main container view for the input plate.
- (void)setupInputPlateContainerView {
  _inputPlateContainerView = [[UIView alloc] init];
  _inputPlateContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlateContainerView.backgroundColor = _theme.inputPlateBackgroundColor;
  _inputPlateContainerView.layer.cornerRadius = kInputPlateCornerRadius;

  _inputPlateInternalContainerView = [[UIView alloc] init];
  _inputPlateInternalContainerView.clipsToBounds = YES;
  _inputPlateInternalContainerView.layer.cornerRadius = kInputPlateCornerRadius;
  _inputPlateInternalContainerView.translatesAutoresizingMaskIntoConstraints =
      NO;
  [_inputPlateContainerView addSubview:_inputPlateInternalContainerView];
  AddSameConstraints(_inputPlateInternalContainerView,
                     _inputPlateContainerView);

  [self updateDepthShadowAppearance];

  // TODO(crbug.com/475834813): Add a glow effect when dragging an item
  // over the composebox.
  [_inputPlateContainerView
      addInteraction:[[UIDropInteraction alloc] initWithDelegate:self]];
  [self.view addSubview:_inputPlateContainerView];

  _glowEffectView = ios::provider::CreateGlowEffect(
      CGRectZero, kInputPlateCornerRadius, /*glowWidth is deprecated*/ 0);
  if (_glowEffectView) {
    _glowEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    _glowEffectView.userInteractionEnabled = NO;
    [self.view insertSubview:_glowEffectView
                aboveSubview:_inputPlateContainerView];
    AddSameConstraints(_inputPlateContainerView, _glowEffectView);
  }
}

/// Updates the content and layout of the input plate stack view based on the
/// current mode (compact or expanded).
- (void)updateInputPlateStackViewContent {
  for (UIView* arrangedSubview in _inputPlateStackView.arrangedSubviews) {
    if (arrangedSubview != _omniboxContainer) {
      [_inputPlateStackView removeArrangedSubview:arrangedSubview];
      [arrangedSubview removeFromSuperview];
    }
  }
  _toolbarView = nil;

  if (self.compact) {
    [_inputPlateStackView insertArrangedSubview:_plusButton atIndex:0];
    [_inputPlateStackView addArrangedSubview:_micButton];
    [_inputPlateStackView addArrangedSubview:_visualSearchButton];

    _inputPlateStackView.axis = UILayoutConstraintAxisHorizontal;
    _inputPlateStackView.spacing = 0;
    [_inputPlateStackView setCustomSpacing:kButtonsCompactSpacing
                                 afterView:_plusButton];
    [_inputPlateStackView setCustomSpacing:kShortcutsSpacing
                                 afterView:_micButton];
    _bottomPaddingConstraint.constant = -kInputPlateStackViewVerticalPadding;
  } else {
    _toolbarView = [self createToolbarView];
    [_inputPlateStackView insertArrangedSubview:_carouselContainer atIndex:0];
    [_inputPlateStackView addArrangedSubview:_toolbarView];
    _inputPlateStackView.axis = UILayoutConstraintAxisVertical;
    _inputPlateStackView.spacing = kInputPlateStackViewSpacing;
    // `_bottomPaddingConstraint` is updated in `updateToolbarVisibility`.
    [self updateToolbarVisibility];
    _inputPlateContainerView.layer.cornerRadius = kInputPlateCornerRadius;
    _inputPlateInternalContainerView.layer.cornerRadius =
        kInputPlateCornerRadius;
  }

  [self updateInputPlateStackViewPadding];
  [self updateInputPlateStackViewTopConstraint];
}

// Updates the side paddings of the input plate stack view.
- (void)updateInputPlateStackViewPadding {
  if (self.compact) {
    CGFloat trailingPadding = kInputPlatePadding.trailing;
    ComposeboxInputPlateControls shortcuts =
        ComposeboxInputPlateControls::kLens |
        ComposeboxInputPlateControls::kVoice;
    BOOL shortcutsVisible =
        (_visibleControls & shortcuts) != ComposeboxInputPlateControls::kNone;
    if (shortcutsVisible) {
      trailingPadding += kShortcutsTrailingPaddingCompact;
    }

    _inputPlateStackView.layoutMarginsRelativeArrangement = YES;
    // Ensure we do not lose the margins on the sides when in compact mode.
    _inputPlateStackView.layoutMargins =
        UIEdgeInsetsMake(0, kInputPlatePadding.leading, 0, trailingPadding);
    // Margins are applied on the input plate, remove the margins on the
    // omnibox.
    _omniboxContainer.directionalLayoutMargins = NSDirectionalEdgeInsetsZero;
  } else {
    _inputPlateStackView.layoutMarginsRelativeArrangement = NO;
    _omniboxContainer.directionalLayoutMargins = kInputPlatePadding;
  }
}

/// Animates the transition of the input plate stack view between compact and
/// expanded states.
- (void)updateInputPlateStackViewAnimated:(BOOL)animated {
  if (!animated) {
    [self updateInputPlateStackViewContent];
    [self updatePreferredContentSize];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kCompactModeAnimationDuration
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        [self updateInputPlateStackViewContent];
        [self.inputPlateStackView layoutIfNeeded];
        [self.view layoutIfNeeded];
      }
      completion:^(BOOL complete) {
        if (complete) {
          [weakSelf updatePreferredContentSize];
        }
      }];
}

// Updates the preferred content size based on the input plate content height.
- (void)updatePreferredContentSize {
  CGFloat inputHeight = [self inputHeight];
  self.preferredContentSize =
      CGSizeMake(self.view.bounds.size.width, inputHeight);
}

/// Generates a banana icon image to be used in the UI.
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

// Returns a base configuration for a mode indicator button.
- (UIButtonConfiguration*)modeIndicatorButtonConfigWithTitle:(NSString*)title
                                                       image:(UIImage*)image {
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.imagePadding = 5;
  config.image = image;
  UIFont* font = [UIFont systemFontOfSize:kAIMButtonFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  config.attributedTitle =
      [[NSAttributedString alloc] initWithString:title attributes:attributes];
  return config;
}

- (UIButton*)createImageGenerationButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.configurationUpdateHandler =
      [self configurationUpdateHandlerForModeIndicator];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityIdentifier =
      kComposeboxImageGenerationButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(imageGenerationButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  button.layer.borderWidth = 0;

  NSString* title = [self titleFor:InputPlateString::kImageGeneration
                              type:InputPlateStringType::kChipLabel];
  UIButtonConfiguration* config =
      [self modeIndicatorButtonConfigWithTitle:title image:[self bananaIcon]];
  config.contentInsets = kImageGenerationButtonInsets;
  config.background.backgroundColor =
      [_theme imageGenerationButtonBackgroundColor];
  config.baseForegroundColor = [_theme imageGenerationButtonTextColor];
  button.tintColor = [_theme imageGenerationButtonTextColor];

  button.configuration = config;
  [self setupXMarkInButton:button];

  return button;
}

// Uptates the create image nudge button title.
- (void)updateCreateImageTitle {
  UIButtonConfiguration* config = _imageGenerationButton.configuration;

  NSString* createImageTitle = [self titleFor:InputPlateString::kImageGeneration
                                         type:InputPlateStringType::kChipLabel];
  UIFont* font = [UIFont systemFontOfSize:kAIMButtonFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes = @{NSFontAttributeName : font};

  config.attributedTitle =
      [[NSAttributedString alloc] initWithString:createImageTitle
                                      attributes:attributes];

  _imageGenerationButton.configuration = config;
}

- (NSString*)serverStringForElement:(InputPlateString)element
                               type:(InputPlateStringType)type {
  using enum InputPlateString;

  ComposeboxServerStringBundle* serverBundle;
  switch (element) {
    case kAIM:
      // AIM always falls back to local default.
      return nil;
    case kToolsSection:
      return _serverStrings.toolsSectionHeader;
    case kModelsSection:
      return _serverStrings.modelSectionHeader;
    case kImageGeneration:
      serverBundle = [_serverStrings
          stringsForControl:ComposeboxInputPlateControls::kCreateImage];
      break;
    case kCanvas:
      serverBundle = [_serverStrings
          stringsForControl:ComposeboxInputPlateControls::kCanvas];
      break;
    case kDeepSearch:
      serverBundle = [_serverStrings
          stringsForControl:ComposeboxInputPlateControls::kDeepSearch];
      break;
    case kRegularModel:
      serverBundle =
          [_serverStrings stringsForModel:ComposeboxModelOption::kRegular];
      break;
    case kAutoModel:
      serverBundle =
          [_serverStrings stringsForModel:ComposeboxModelOption::kAuto];
      break;
    case kThinkingModel:
      serverBundle =
          [_serverStrings stringsForModel:ComposeboxModelOption::kThinking];
      break;
  }

  switch (type) {
    case InputPlateStringType::kMenuLabel:
      return serverBundle.menuLabel;
    case InputPlateStringType::kChipLabel:
      return serverBundle.chipLabel;
    case InputPlateStringType::kHintText:
      return serverBundle.hintText;
  }
}

- (NSString*)localFallbackForElement:(InputPlateString)element
                                type:(InputPlateStringType)type {
  using enum InputPlateString;
  using enum InputPlateStringType;

  switch (element) {
    case kAIM:
      return type == kHintText
                 ? l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_AIM_ENABLED_PLACEHOLDER)
                 : l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION);
    case kToolsSection:
      return @"";
    case kModelsSection:
      return l10n_util::GetNSStringF(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE,
                                     base::SysNSStringToUTF16(@"3"));
    case kImageGeneration:
      return type == kHintText ? l10n_util::GetNSString(
                                     IDS_IOS_COMPOSEBOX_IMAGE_GEN_PLACEHOLDER)
                               : l10n_util::GetNSString(
                                     IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION);
    case kCanvas:
      return type == kHintText
                 ? l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_CANVAS_ENABLED_PLACEHOLDER)
                 : l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION);
    case kDeepSearch:
      return type == kHintText
                 ? l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ENABLED_PLACEHOLDER)
                 : l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION);
    case kRegularModel:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO);
    case kAutoModel:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO);
    case kThinkingModel:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING);
  }
}

// Returns the title for the given input plate element of a certain type.
- (NSString*)titleFor:(InputPlateString)element
                 type:(InputPlateStringType)type {
  return [self serverStringForElement:element type:type]
             ?: [self localFallbackForElement:element type:type];
}

// Creates a new canvas button to be displayed in the input plate.
- (UIButton*)createCanvasButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.configurationUpdateHandler =
      [self configurationUpdateHandlerForModeIndicator];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(canvasButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  button.layer.borderWidth = 0;

  NSString* title = [self titleFor:InputPlateString::kCanvas
                              type:InputPlateStringType::kChipLabel];
  UIButtonConfiguration* config =
      [self modeIndicatorButtonConfigWithTitle:title
                                         image:CustomSymbolWithPointSize(
                                                   kDocumentBadgeSpark,
                                                   kAIMButtonSymbolPointSize)];
  NSDirectionalEdgeInsets insets = kModeIndicatorButtonInsets;
  insets.trailing = kModeIndicatorButtonInsets.trailing + kXButtonWidthInButton;
  config.contentInsets = insets;

  config.background.backgroundColor = [_theme canvasButtonBackgroundColor];
  config.baseForegroundColor = [_theme canvasButtonTextColor];
  button.tintColor = [_theme canvasButtonTextColor];

  button.configuration = config;

  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor
        constraintGreaterThanOrEqualToConstant:kAIMButtonBaseWidth +
                                               kXButtonWidthInButton]
  ]];

  [self setupXMarkInButton:button];

  return button;
}

// Creates a new deep search button to be displayed in the input plate.
- (UIButton*)createDeepSearchButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.configurationUpdateHandler =
      [self configurationUpdateHandlerForModeIndicator];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(deepSearchButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  button.layer.borderWidth = 0;

  NSString* title = [self titleFor:InputPlateString::kDeepSearch
                              type:InputPlateStringType::kChipLabel];
  UIButtonConfiguration* config =
      [self modeIndicatorButtonConfigWithTitle:title
                                         image:CustomSymbolWithPointSize(
                                                   kDeepSearchSymbol,
                                                   kAIMButtonSymbolPointSize)];
  NSDirectionalEdgeInsets insets = kModeIndicatorButtonInsets;
  insets.trailing = kModeIndicatorButtonInsets.trailing + kXButtonWidthInButton;
  config.contentInsets = insets;

  config.background.backgroundColor = [_theme deepSearchButtonBackgroundColor];
  config.baseForegroundColor = [_theme deepSearchButtonTextColor];
  button.tintColor = [_theme deepSearchButtonTextColor];

  button.configuration = config;

  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor
        constraintGreaterThanOrEqualToConstant:kAIMButtonBaseWidth +
                                               kXButtonWidthInButton]
  ]];

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  [self setupXMarkInButton:button];

  return button;
}

// Creates an 'ask about this page' tab button to be displayed in the input
// plate.
- (UIButton*)createAskAboutThisPageButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.configurationUpdateHandler =
      [self configurationUpdateHandlerForModeIndicator];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(askAboutThisPageButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  button.tintColor = [_theme aimButtonTextColorWithAIMEnabled:NO];

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIButtonConfiguration* config = [self
      modeIndicatorButtonConfigWithTitle:
          l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_ASK_ABOUT_THIS_PAGE_ACTION)
                                   image:CustomSymbolWithPointSize(
                                             kMagnifyingglassSparkSymbol,
                                             kAIMButtonSymbolPointSize)];
  config.background.backgroundColor = [UIColor clearColor];
  config.baseForegroundColor = [_theme aimButtonTextColorWithAIMEnabled:NO];
  config.background.strokeWidth = 1;
  config.background.strokeColor =
      [_theme aimButtonBorderColorWithAIMEnabled:NO];

  button.configuration = config;

  return button;
}

// Returns the configuration update handler animating the button on tap.
- (UIButtonConfigurationUpdateHandler)
    configurationUpdateHandlerForModeIndicator {
  return ^(UIButton* updatedButton) {
    BOOL isHighlighted = updatedButton.state == UIControlStateHighlighted;
    CGFloat scale = isHighlighted ? 0.95 : 1.0;
    CGFloat alpha = isHighlighted ? 0.85 : 1.0;
    [UIView animateWithDuration:0.1
                     animations:^{
                       updatedButton.alpha = alpha;
                       updatedButton.transform =
                           CGAffineTransformMakeScale(scale, scale);
                     }];
  };
}

/// Called when a drop session ends.
- (void)dropSessionDidEnd:(id<UIDropSession>)session {
  CHECK(self.delegate);

  if (!_dragSessionWithinInputPlate) {
    return;
  }

  if ([self isDropAllowed:session]) {
    return;
  }

  // Drop to attach was not allowed because the set of items dropped was not
  // valid.
  [self.delegate didFailToAttachDueToIneligibleAttachments:self];
}

/// Returns whether a drop action will be allowed for a given drop session.
- (BOOL)isDropAllowed:(id<UIDropSession>)session {
  if (session.items.count > _remainingAttachmentCapacity) {
    // Text drops are always allowed even if the attachment capacity is reached.
    return [self willAllowTextDrop:session];
  }

  BOOL willAllowPDFDrop = [self willAllowPDFDrop:session];
  BOOL willAllowImageDrop = [self willAllowImageDrop:session];
  BOOL willAllowTabDrop = [self willAllowTabDrop:session];
  BOOL willAllowTextDrop = [self willAllowTextDrop:session];

  return willAllowPDFDrop || willAllowImageDrop || willAllowTabDrop ||
         willAllowTextDrop;
}

/// Returns whether a text drop will be allowed.
- (BOOL)willAllowTextDrop:(id<UIDropSession>)session {
  return
      [session hasItemsConformingToTypeIdentifiers:@[ UTTypeText.identifier ]];
}

/// Returns whether an image drop will be allowed based on the Composebox mode,
/// whether a drag and drop action is allowed, and whether there is an image in
/// the drop session.
- (BOOL)willAllowImageDrop:(id<UIDropSession>)session {
  if (_galleryActionsDisabled || _galleryActionsHidden) {
    return NO;
  }

  if (![session
          hasItemsConformingToTypeIdentifiers:@[ UTTypeImage.identifier ]]) {
    return NO;
  }

  return YES;
}

/// Returns whether a tab drop will be allowed.
- (BOOL)willAllowTabDrop:(id<UIDropSession>)session {
  if (_attachTabActionsDisabled || _attachTabActionsHidden) {
    return NO;
  }

  for (UIDragItem* item in session.items) {
    if ([item.localObject isKindOfClass:[TabInfo class]]) {
      // Disallow tab drops between profiles and between incognito and
      // non-incognito sessions.
      TabInfo* tab = item.localObject;

      if ([self.delegate tabExistsOnCurrentProfile:tab]) {
        return YES;
      }
    }
  }

  return NO;
}

/// Returns whether a PDF drop will be allowed based on the Composebox mode,
/// whether a drag and drop action is allowed, and whether there is a PDF in the
/// drop session.
- (BOOL)willAllowPDFDrop:(id<UIDropSession>)session {
  if (_attachFileActionsDisabled || _attachFileActionsHidden) {
    return NO;
  }

  if (![session
          hasItemsConformingToTypeIdentifiers:@[ UTTypePDF.identifier ]]) {
    return NO;
  }

  return YES;
}

/// Performs a drop action for a given drop session.
- (void)performDrop:(id<UIDropSession>)session {
  base::RecordAction(
      base::UserMetricsAction("IOS.Omnibox.MobileFusebox.Action.DragAndDrop"));
  // Drop each eligible dragged item into the Composebox.
  for (UIDragItem* item in session.items) {
    if ([self willAllowPDFDrop:session] &&
        [item.itemProvider
            hasItemConformingToTypeIdentifier:UTTypePDF.identifier]) {
      [self.delegate composeboxViewController:self
                    didAttemptDragAndDropType:ComposeboxDragAndDropType::kPDF];
      [self performDropForPDF:item.itemProvider];
    } else if ([self willAllowImageDrop:session] &&
               [item.itemProvider
                   hasItemConformingToTypeIdentifier:UTTypeImage.identifier]) {
      [self.delegate
           composeboxViewController:self
          didAttemptDragAndDropType:ComposeboxDragAndDropType::kImage];
      [self performDropForImage:item.itemProvider];
    } else if ([self willAllowTabDrop:session] &&
               [item.localObject isKindOfClass:[TabInfo class]]) {
      [self.delegate composeboxViewController:self
                    didAttemptDragAndDropType:ComposeboxDragAndDropType::kTab];
      [self performDropForTab:item.localObject];
    } else if ([self willAllowTextDrop:session] &&
               [item.itemProvider
                   hasItemConformingToTypeIdentifier:UTTypeText.identifier]) {
      [self.delegate composeboxViewController:self
                    didAttemptDragAndDropType:ComposeboxDragAndDropType::kText];
      [self performDropForText:item.itemProvider];
    } else {
      [self.delegate
           composeboxViewController:self
          didAttemptDragAndDropType:ComposeboxDragAndDropType::kUnknown];
    }
  }
  // Drop complete.
  _dragSessionWithinInputPlate = NO;
}

/// Performs a drop for dragged text from a given `itemProvider`.
- (void)performDropForText:(NSItemProvider*)itemProvider {
  CHECK([itemProvider hasItemConformingToTypeIdentifier:UTTypeText.identifier]);

  __weak __typeof(self) weakSelf = self;
  [itemProvider loadObjectOfClass:[NSString class]
                completionHandler:^(NSString* text, NSError* error) {
                  [weakSelf handleTextDrop:text error:error];
                }];
}

/// Performs a drop for a dragged tab with `tabInfo`.
- (void)performDropForTab:(TabInfo*)tabInfo {
  CHECK(self.mutator);
  CHECK(tabInfo);
  CHECK_EQ(tabInfo.incognito, _theme.incognito);

  web::WebState* webState =
      [self.delegate webStateForTabOnCurrentProfile:tabInfo];

  if (!webState) {
    return;
  }

  [self.mutator processTab:webState webStateID:tabInfo.tabID];
}

/// Performs a drop for a dragged image file from a given `itemProvider`.
- (void)performDropForImage:(NSItemProvider*)itemProvider {
  CHECK(self.mutator);
  CHECK(
      [itemProvider hasItemConformingToTypeIdentifier:UTTypeImage.identifier]);

  [self.mutator processImageItemProvider:itemProvider
                                 assetID:[NSUUID UUID].UUIDString];
}

/// Performs a drop for a dragged PDF file from a given `itemProvider`.
- (void)performDropForPDF:(NSItemProvider*)itemProvider {
  CHECK([itemProvider hasItemConformingToTypeIdentifier:UTTypePDF.identifier]);

  __weak __typeof(self) weakSelf = self;
  [itemProvider
      loadFileRepresentationForTypeIdentifier:UTTypePDF.identifier
                            completionHandler:^(NSURL* url, NSError* error) {
                              [weakSelf handlePDFDrop:url error:error];
                            }];
}

/// Helper for `-performDropForText`. handles a drop action for dragged text.
- (void)handleTextDrop:(NSString*)text error:(NSError*)error {
  CHECK(self.mutator);

  if (error || !text) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.mutator processText:text];
  });
}

/// Helper for `-performDropForPDF`. Handles a drop action for a PDF file.
- (void)handlePDFDrop:(NSURL*)url error:(NSError*)error {
  CHECK(self.mutator);

  if (error || !url) {
    return;
  }

  NSString* fileName = url.lastPathComponent;
  NSURL* tempDirectory = [NSFileManager.defaultManager temporaryDirectory];
  NSURL* destinationURL = [tempDirectory URLByAppendingPathComponent:fileName];
  NSFileManager* fileManager = [NSFileManager defaultManager];

  // Remove existing file at the destination if it exists, so we can overwrite
  // it.
  if ([fileManager fileExistsAtPath:destinationURL.path]) {
    [fileManager removeItemAtURL:destinationURL error:nil];
  }

  if (![fileManager copyItemAtURL:url toURL:destinationURL error:nil]) {
    return;
  }

  GURL pdfURL = net::GURLWithNSURL(destinationURL);

  if (!pdfURL.is_valid()) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.mutator processPDFFileURL:pdfURL];
  });
}

@end
