// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller.h"

#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller_delegate.h"
#import "ios/chrome/browser/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (void)addContent:(UIViewController*)content {
  [self addChildViewController:content];
  content.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentContainer addSubview:content.view];
  AddSameConstraints(self.contentContainer, content.view);
  [content didMoveToParentViewController:self];
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

@end
