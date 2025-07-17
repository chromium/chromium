// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AIMPrototypeViewController

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
  UITextView* textView = [[UITextView alloc] init];
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  textView.text = @"Ask anything";
  textView.backgroundColor = UIColor.clearColor;
  [textView.heightAnchor constraintEqualToConstant:40].active = YES;

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

  UIButton* sendButton = [self
      createButtonWithImage:DefaultSymbolWithPointSize(@"arrow.up.circle.fill",
                                                       kSymbolActionPointSize)];
  [sendButton addTarget:self
                 action:@selector(sendButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];

  // Horizontal stack view for buttons
  UIStackView* buttonsStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        galleryButton, lensButton, [UIView new], micButton, sendButton
      ]];
  buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonsStackView.spacing = 16;
  buttonsStackView.alignment = UIStackViewAlignmentCenter;

  // Main vertical stack view
  UIStackView* mainStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ textView, buttonsStackView ]];
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

#pragma mark - Actions

- (void)closeButtonTapped {
  [self.delegate aimPrototypeViewControllerDidTapCloseButton:self];
}

- (void)galleryButtonTapped {
  // TODO(crbug.com/40280872): Implement gallery action.
}

- (void)lensButtonTapped {
  // TODO(crbug.com/40280872): Implement lens action.
}

- (void)micButtonTapped {
  // TODO(crbug.com/40280872): Implement mic action.
}

- (void)sendButtonTapped {
  // TODO(crbug.com/40280872): Implement send action.
}

#pragma mark - Private

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
