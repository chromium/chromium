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
/// The maximum number of lines for the text view before it starts scrolling.
const int kMaxLines = 5;
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
const CGFloat kCarouselHeight = 70.0f;
/// The height of the AIM mode button.
const CGFloat kAIMButtonHeight = 32.0f;
/// The width of the AIM mode button.
const CGFloat kAIMButtonWidth = 94.0f;
/// The spacing for the horizontal buttons stack view.
const CGFloat kButtonsStackViewSpacing = 18.0f;
/// The spacing for the main vertical input plate stack view.
const CGFloat kInputPlateStackViewSpacing = 6.0f;
/// The padding for the close button.
const CGFloat kCloseButtonPadding = 16.0f;
/// The horizontal and bottom padding for the input plate container.
const CGFloat kInputPlatePadding = 10.0f;
/// The vertical padding for the input plate stack view.
const CGFloat kInputPlateStackViewVerticalPadding = 10.0f;
/// The leading padding for the input plate stack view.
const CGFloat kInputPlateStackViewLeadingPadding = 20.0f;
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
}

@interface AIMPrototypeViewController () <UITextViewDelegate>

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
  /// The label used as a placeholder for the text view.
  UILabel* _placeholderLabel;
  /// The constraint for the height of the text view.
  NSLayoutConstraint* _textViewHeightConstraint;
  /// The text view for user input.
  UITextView* _textView;
  /// The button to toggle AI mode.
  UIButton* _aimButton;
  /// The glow effect around the input plate container.
  UIView<GlowEffect>* _glowEffectView;

  /// Container for the omnibox.
  UIView* _omniboxContainer;
  /// Container for the omnibox popup.
  UIView* _omniboxPopupContainer;
}

/// AIMPrototypeAnimationContextProvider
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;
@synthesize textViewForAnimation = _textView;

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
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeClose];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:closeButton];

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
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_omniboxPopupContainer
              belowSubview:_inputPlateContainerView];

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

  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.estimatedItemSize = UICollectionViewFlowLayoutAutomaticSize;
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.hidden = YES;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[AIMInputItemCell class]
      forCellWithReuseIdentifier:kItemCellReuseIdentifier];
  _dataSource = [self createDataSource];
  _carouselView.dataSource = _dataSource;
  [_carouselView.heightAnchor constraintEqualToConstant:kCarouselHeight]
      .active = YES;
  _carouselView.showsHorizontalScrollIndicator = NO;

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

  UIButton* micButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                       kSymbolActionPointSize)];
  [micButton addTarget:self
                action:@selector(micButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];

  // Horizontal stack view for buttons
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        plusButton, _aimButton, [UIView new], micButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = kButtonsStackViewSpacing;
  buttonsStackView.alignment = UIStackViewAlignmentBottom;

  // Main vertical stack view
  _inputPlateStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _omniboxContainer, _carouselView, buttonsStackView
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
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
}

#pragma mark - AIMPrototypeConsumer

- (void)setItems:(NSArray<AIMInputItem*>*)items {
  _carouselView.hidden = items.count == 0;
  NSDiffableDataSourceSnapshot<NSString*, AIMInputItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
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

#pragma mark - UITextViewDelegate

// The final implementation in the omnibox is expected to use a UIKeyCommand
// override to handle the return key. This delegate method is a temporary
// solution for this prototype.
- (BOOL)textView:(UITextView*)textView
    shouldChangeTextInRange:(NSRange)range
            replacementText:(NSString*)text {
  if ([text isEqualToString:@"\n"]) {
    [self.mutator sendText:textView.text];
    textView.text = @"";
    // Manually trigger textViewDidChange to update placeholder and layout.
    [self textViewDidChange:textView];
    return NO;
  }
  return YES;
}

- (void)textViewDidChange:(UITextView*)textView {
  _placeholderLabel.hidden = textView.hasText;

  // Recalculate textView height and update it to clip and scroll if necessary.
  CGFloat verticalPadding =
      _textView.textContainerInset.top + _textView.textContainerInset.bottom;
  CGFloat maxHeight = (_textView.font.lineHeight * kMaxLines) + verticalPadding;
  CGSize size = [_textView
      sizeThatFits:CGSizeMake(_textView.frame.size.width, CGFLOAT_MAX)];
  CGFloat newHeight = MIN(size.height, maxHeight);
  _textViewHeightConstraint.constant = newHeight;
  _textView.scrollEnabled = size.height > maxHeight;
  [self.view layoutIfNeeded];
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
