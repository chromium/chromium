// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

namespace {

// The close button top padding.
const CGFloat closeButtonTopPadding = 10.0;
// The close button trailing padding.
const CGFloat closeButtonTrailingPadding = 16.0;

}  // namespace

@implementation LensOverlayContainerViewController {
  // The overlay commands handler.
  id<LensOverlayCommands> _overlayCommandsHandler;
  // The overlay close button.
  UIButton* _closeButton;
}

- (instancetype)initWithLensOverlayCommandsHandler:
    (id<LensOverlayCommands>)handler {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _overlayCommandsHandler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
  self.view.accessibilityIdentifier = kLenscontainerViewAccessibilityIdentifier;

  self.closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:self.closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [self.closeButton.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:closeButtonTopPadding],
    [self.closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-closeButtonTrailingPadding]
  ]];

  if (!self.selectionViewController) {
    return;
  }

  [self addChildViewController:self.selectionViewController];
  [self.view addSubview:self.selectionViewController.view];

  self.selectionViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.selectionViewController.view.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:80.0f],
    [self.selectionViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-80.0f],
    [self.selectionViewController.view.leftAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leftAnchor],
    [self.selectionViewController.view.rightAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.rightAnchor],
  ]];

  [self.selectionViewController didMoveToParentViewController:self];
}

- (UIButton*)closeButton {
  if (_closeButton) {
    return _closeButton;
  }

  _closeButton = [[UIButton alloc] init];

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:20
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleLarge];

  configuration.image = SymbolWithPalette(
      DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                     symbolConfiguration),
      @[ UIColor.whiteColor, [UIColor colorWithWhite:0 alpha:0.2] ]);
  _closeButton.configuration = configuration;

  [_closeButton addTarget:self
                   action:@selector(closeButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  _closeButton.accessibilityIdentifier =
      kLenscontainerViewCloseButtonAccessibilityIdentifier;

  return _closeButton;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self closeButtonPressed];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self escapeButtonPressed];
}

#pragma mark - Actions

- (void)closeButtonPressed {
  [_overlayCommandsHandler destroyLensUI:YES];
}

- (void)escapeButtonPressed {
  [self closeButtonPressed];
}

@end
