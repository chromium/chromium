// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
NSString* const kImageCellReuseIdentifier = @"ImageCellReuseIdentifier";
const int kMaxLines = 5;
}

@interface AIMPrototypeViewController () <UICollectionViewDataSource,
                                          UITextViewDelegate>
@end

@implementation AIMPrototypeViewController {
  UICollectionView* _carouselView;
  NSArray<UIImage*>* _images;
  UILabel* _placeholderLabel;
  NSLayoutConstraint* _textViewHeightConstraint;
  UITextView* _textView;
  UIButton* _sendButton;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // Close button
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeClose];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:closeButton];

  // --- Bottom Input Area ---

  // Input plate container
  UIView* inputPlateView = [[UIView alloc] init];
  inputPlateView.translatesAutoresizingMaskIntoConstraints = NO;
  inputPlateView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  inputPlateView.layer.cornerRadius = 20;
  inputPlateView.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  [self.view addSubview:inputPlateView];

  // Text view
  _textView = [[UITextView alloc] init];
  _textView.translatesAutoresizingMaskIntoConstraints = NO;
  _textView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  _textView.backgroundColor = UIColor.clearColor;
  _textView.delegate = self;
  _textView.scrollEnabled = NO;

  CGFloat verticalPadding =
      _textView.textContainerInset.top + _textView.textContainerInset.bottom;
  CGFloat singleLineHeight = _textView.font.lineHeight + verticalPadding;
  _textViewHeightConstraint =
      [_textView.heightAnchor constraintEqualToConstant:singleLineHeight];
  _textViewHeightConstraint.active = YES;

  _placeholderLabel = [[UILabel alloc] init];
  _placeholderLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/40280872): Localize this string.
  _placeholderLabel.text = @"Ask anything";
  _placeholderLabel.font = _textView.font;
  _placeholderLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [_textView addSubview:_placeholderLabel];
  [NSLayoutConstraint activateConstraints:@[
    [_placeholderLabel.topAnchor constraintEqualToAnchor:_textView.topAnchor
                                                constant:8],
    [_placeholderLabel.leadingAnchor
        constraintEqualToAnchor:_textView.leadingAnchor
                       constant:5],
  ]];

  // Carousel view
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.itemSize = CGSizeMake(48, 48);
  layout.minimumLineSpacing = 12;
  _carouselView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                     collectionViewLayout:layout];
  _carouselView.dataSource = self;
  _carouselView.translatesAutoresizingMaskIntoConstraints = NO;
  _carouselView.hidden = YES;
  _carouselView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  [_carouselView registerClass:[UICollectionViewCell class]
      forCellWithReuseIdentifier:kImageCellReuseIdentifier];
  [_carouselView.heightAnchor constraintEqualToConstant:48].active = YES;
  _carouselView.showsHorizontalScrollIndicator = NO;

  // Action buttons
  UIButton* galleryButton =
      [self createButtonWithImage:DefaultSymbolWithPointSize(
                                      kPhotoSymbol, kSymbolActionPointSize)];
  [galleryButton addTarget:self
                    action:@selector(galleryButtonTapped)
          forControlEvents:UIControlEventTouchUpInside];

  UIButton* lensButton = [self
      createButtonWithImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                      kSymbolActionPointSize)];
  [lensButton addTarget:self
                 action:@selector(lensButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];

  UIButton* micButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                       kSymbolActionPointSize)];
  [micButton addTarget:self
                action:@selector(micButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];

  _sendButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(@"arrow.up.circle.fill",
                                                       kSymbolActionPointSize)];
  [_sendButton addTarget:self
                  action:@selector(sendButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  _sendButton.enabled = NO;

  // Horizontal stack view for buttons
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        galleryButton, lensButton, [UIView new], micButton, _sendButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = 16;
  buttonsStackView.alignment = UIStackViewAlignmentCenter;

  // Main vertical stack view
  UIStackView* mainStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textView, _carouselView, buttonsStackView ]];
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.spacing = 8;
  [inputPlateView addSubview:mainStackView];

  // Layout
  [NSLayoutConstraint activateConstraints:@[
    // Close button
    [closeButton.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:16],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-16],

    // Input Plate
    [inputPlateView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [inputPlateView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [inputPlateView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],

    // Main Stack View in Plate
    [mainStackView.topAnchor constraintEqualToAnchor:inputPlateView.topAnchor
                                            constant:8],
    [mainStackView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-8],
    [mainStackView.leadingAnchor
        constraintEqualToAnchor:inputPlateView.leadingAnchor
                       constant:16],
    [mainStackView.trailingAnchor
        constraintEqualToAnchor:inputPlateView.trailingAnchor
                       constant:-16],
  ]];
}

#pragma mark - AIMPrototypeConsumer

- (void)setImages:(NSArray<UIImage*>*)images {
  _images = images;
  _carouselView.hidden = _images.count == 0;
  [_carouselView reloadData];
  [self updateSendButtonState];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _images.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kImageCellReuseIdentifier
                                forIndexPath:indexPath];
  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:_images[indexPath.row]];
  imageView.contentMode = UIViewContentModeScaleAspectFill;
  cell.backgroundView = imageView;
  cell.layer.cornerRadius = 16;
  cell.clipsToBounds = YES;
  return cell;
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  _placeholderLabel.hidden = textView.hasText;
  [self updateSendButtonState];

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

- (void)lensButtonTapped {
  // TODO(crbug.com/40280872): Implement lens action.
}

- (void)micButtonTapped {
  // TODO(crbug.com/40280872): Implement mic action.
}

- (void)sendButtonTapped {
  [self.mutator sendText:_textView.text];
}

#pragma mark - Private

- (void)updateSendButtonState {
  _sendButton.enabled = _textView.hasText || _images.count > 0;
}

- (UIButton*)createButtonWithImage:(UIImage*)image {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button.widthAnchor constraintEqualToConstant:28].active = YES;
  [button.heightAnchor constraintEqualToConstant:28].active = YES;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  return button;
}

@end
