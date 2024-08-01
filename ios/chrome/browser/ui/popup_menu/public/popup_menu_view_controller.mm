// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kImageMargin = 196;
}  // namespace

@interface PopupMenuViewController ()
// Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* contentContainer;
@end

@implementation PopupMenuViewController

@synthesize contentContainer = _contentContainer;
@synthesize delegate = _delegate;

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    UIButton* closeButton =
        [AccessibilityCloseMenuButton buttonWithType:UIButtonTypeCustom];
    [closeButton addTarget:self
                    action:@selector(dismissPopup)
          forControlEvents:UIControlEventTouchUpInside];
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:closeButton];
    AddSameConstraints(self.view, closeButton);
    [self setUpContentContainer];

    self.view.accessibilityViewIsModal = YES;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    closeButton);
  }
  return self;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Let the view controller become first responder to handle key commands.
  // See `-keyCommands`.
  [self becomeFirstResponder];
}

- (void)addContent:(UIViewController*)content {
  [self addChildViewController:content];
  content.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentContainer addSubview:content.view];
  AddSameConstraints(self.contentContainer, content.view);
  [content didMoveToParentViewController:self];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close, UIKeyCommand.cr_stop ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate popupMenuViewControllerWillDismiss:self];
}

- (void)keyCommand_stop {
  // Forward to the keyCommand_close action. Usually this is done by the OS
  // implicitly, but since BVC handles the stop action for stopping loading the
  // page, the PopupMenuViewController needs to explicitly catch it before.
  [self keyCommand_close];
}

#pragma mark - Private

// Sets the content container view up.
- (void)setUpContentContainer {
  _contentContainer = [[UIView alloc] init];
  _contentContainer.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIImageView* shadow =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  [_contentContainer addSubview:shadow];
  shadow.frame =
      CGRectInset(_contentContainer.frame, -kImageMargin, -kImageMargin);
  shadow.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  _contentContainer.layer.cornerRadius = kPopupMenuCornerRadius;
  _contentContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentContainer];
}

// Handler receiving the touch event on the background scrim.
- (void)dismissPopup {
  [self.delegate popupMenuViewControllerWillDismiss:self];
}

- (void)preferredContentSizeDidChangeForChildContentContainer:
    (id<UIContentContainer>)container {
  [self.delegate
      containedViewControllerContentSizeChangedForPopupMenuViewController:self];
}

@end
