// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_image_cell.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller+private.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"

namespace {
/// The reuse identifier for the image cells in the carousel.
NSString* const kImageCellReuseIdentifier = @"AIMImageCell";
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
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
/// The size of the logo image view.
const CGFloat kLogoImageSize = 24.0f;
/// The spacing between the logo and the text view.
const CGFloat kLogoTextViewSpacing = 6.0f;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS
/// The size of the items in the carousel.
const CGFloat kCarouselItemSize = 48.0f;
/// The spacing between items in the carousel.
const CGFloat kCarouselItemSpacing = 12.0f;
/// The height of the carousel view.
const CGFloat kCarouselHeight = 48.0f;
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
  /// The backing view for the animation.
  UIView* _mainViewForAnimation;
  /// The button to toggle AI mode.
  UIButton* _aimButton;
  /// Whether the AI mode is enabled.
  BOOL _aiModeEnabled;
  /// The glow effect around the input plate container.
  UIView<GlowEffect>* _glowEffectView;
}

/// AIMPrototypeAnimationContextProvider
@synthesize mainViewForAnimation = _mainViewForAnimation;
@synthesize inputPlateViewForAnimation = _inputPlateContainerView;
@synthesize textViewForAnimation = _textView;

- (void)viewDidLoad {
  [super viewDidLoad];

  _mainViewForAnimation = self.view;
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

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

  // Text view
  _textView = [[UITextView alloc] init];
  _textView.translatesAutoresizingMaskIntoConstraints = NO;
  _textView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  _textView.backgroundColor = UIColor.clearColor;
  _textView.delegate = self;
  _textView.scrollEnabled = NO;
  _textView.autocapitalizationType = UITextAutocapitalizationTypeNone;
  _textView.autocorrectionType = UITextAutocorrectionTypeNo;
  _textView.spellCheckingType = UITextSpellCheckingTypeNo;
  _textView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _textView.returnKeyType = UIReturnKeyGo;
  _textView.enablesReturnKeyAutomatically = YES;

  // Calculate the initial height of the text view using `sizeThatFits:`. This
  // ensures the initial height is calculated using the exact same logic as the
  // dynamic resizing, which prevents a "jump" when the first character is
  // entered.
  CGFloat initialHeight =
      [_textView
          sizeThatFits:CGSizeMake(_textView.frame.size.width, CGFLOAT_MAX)]
          .height;
  _textViewHeightConstraint =
      [_textView.heightAnchor constraintEqualToConstant:initialHeight];
  _textViewHeightConstraint.active = YES;

  // Container view for the text view and logo.
  UIView* textViewContainer = [[UIView alloc] init];
  textViewContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The placeholder is added to the container behind the text view. Its
  // baseline provides a stable anchor for the logo, solving the issue of the
  // text view's baseline being unavailable when there is no text.
  _placeholderLabel = [[UILabel alloc] init];
  _placeholderLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/40280872): Localize this string.
  _placeholderLabel.text = @"Ask anything";
  _placeholderLabel.font = _textView.font;
  _placeholderLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [textViewContainer addSubview:_placeholderLabel];
  [textViewContainer addSubview:_textView];

  // Align placeholder with the text view's content area by constraining it
  // directly to the text view's frame and then adding the internal insets.
  UIEdgeInsets textInsets = _textView.textContainerInset;
  CGFloat linePadding = _textView.textContainer.lineFragmentPadding;
  [NSLayoutConstraint activateConstraints:@[
    [_placeholderLabel.topAnchor constraintEqualToAnchor:_textView.topAnchor
                                                constant:textInsets.top],
    [_placeholderLabel.leadingAnchor
        constraintEqualToAnchor:_textView.leadingAnchor
                       constant:textInsets.left + linePadding],
  ]];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImageView* logoImageView = [[UIImageView alloc]
      initWithImage:MakeSymbolMulticolor(CustomSymbolWithPointSize(
                        kGoogleIconSymbol, kSymbolActionPointSize))];
  logoImageView.contentMode = UIViewContentModeCenter;
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [textViewContainer addSubview:logoImageView];

  // Layout logo and text view within the container.
  [NSLayoutConstraint activateConstraints:@[
    [logoImageView.leadingAnchor
        constraintEqualToAnchor:textViewContainer.leadingAnchor],
    // Align to the placeholder's vertical center, which is stable and
    // represents the center of the first line of text.
    [logoImageView.centerYAnchor
        constraintEqualToAnchor:_placeholderLabel.centerYAnchor],
    [logoImageView.widthAnchor constraintEqualToConstant:kLogoImageSize],
    [logoImageView.heightAnchor constraintEqualToConstant:kLogoImageSize],

    [_textView.leadingAnchor
        constraintEqualToAnchor:logoImageView.trailingAnchor
                       constant:kLogoTextViewSpacing],
    [_textView.trailingAnchor
        constraintEqualToAnchor:textViewContainer.trailingAnchor],
    [_textView.topAnchor constraintEqualToAnchor:textViewContainer.topAnchor],
    [_textView.bottomAnchor
        constraintEqualToAnchor:textViewContainer.bottomAnchor],
  ]];
#else
  // If no logo, text view fills the container.
  [NSLayoutConstraint activateConstraints:@[
    [_textView.leadingAnchor
        constraintEqualToAnchor:textViewContainer.leadingAnchor],
    [_textView.trailingAnchor
        constraintEqualToAnchor:textViewContainer.trailingAnchor],
    [_textView.topAnchor constraintEqualToAnchor:textViewContainer.topAnchor],
    [_textView.bottomAnchor
        constraintEqualToAnchor:textViewContainer.bottomAnchor],
  ]];
#endif

  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.itemSize = CGSizeMake(kCarouselItemSize, kCarouselItemSize);
  layout.minimumLineSpacing = kCarouselItemSpacing;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.hidden = YES;
  _carouselView.backgroundColor = UIColor.clearColor;
  [_carouselView registerClass:[AIMImageCell class]
      forCellWithReuseIdentifier:kImageCellReuseIdentifier];
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

  plusButton.menu = [UIMenu menuWithTitle:@""
                                 children:@[
                                   fileAction,
                                   galleryAction,
                                   cameraAction,
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
    textViewContainer, _carouselView, buttonsStackView
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
    if (item.fileToken == token) {
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
  _aiModeEnabled = !_aiModeEnabled;
  [self updateAIMButtonAppearance];
  [self.mutator setAIModeEnabled:_aiModeEnabled];

  if (_aiModeEnabled && _glowEffectView) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self
                                             selector:@selector(stopGlowEffect)
                                               object:nil];
    [_glowEffectView startGlow];
    [self performSelector:@selector(stopGlowEffect)
               withObject:nil
               afterDelay:kGlowEffectDuration];
  }
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

- (UICollectionViewDiffableDataSource<NSString*, AIMInputItem*>*)
    createDataSource {
  return [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_carouselView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    AIMInputItem* item) {
                  AIMImageCell* cell = (AIMImageCell*)[collectionView
                      dequeueReusableCellWithReuseIdentifier:
                          kImageCellReuseIdentifier
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

  if (_aiModeEnabled) {
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
