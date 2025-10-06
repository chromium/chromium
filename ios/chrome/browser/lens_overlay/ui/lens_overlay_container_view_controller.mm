// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_accessibility_identifier_constants.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_panel.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The width of the side panel.
const CGFloat kSidePanelWidth = 375.0;

// The ammount padding from the side panel to the selection UI.
const CGFloat kSidePanelSelectionPadding = 24.0;

// The duration of the side panel appear and dissapear animations.
const CGFloat kSidePannelAnimationDuration = 0.4;

// The corner radius of the selection UI when presented in the side panel
// presentation.
const CGFloat kSelectionUICornerRadius = 13.0;

}  // namespace

@interface LensOverlayContainerViewController () <LensOverlayPanTrackerDelegate>

// Whether the side panel is open.
@property(nonatomic, getter=isSidePanelOpen) BOOL sidePanelOpen;

// Tracks the pan inside the bottom sheet.
@property(nonatomic, readonly) LensOverlayPanTracker* panTracker;

// The current height of the bottom sheet, in points.
@property(nonatomic) CGFloat bottomSheetHeight;

@property(nonatomic, strong) NSLayoutConstraint* bottomSheetHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetBottomConstraint;

@end

@implementation LensOverlayContainerViewController {
  // The overlay commands handler.
  id<LensOverlayCommands> _overlayCommandsHandler;
  // The overlay close button.
  UIButton* _closeButton;
  // View that blocks user interaction with selection UI when the consent view
  // controller is displayed. Note that selection UI isn't started, so it won't
  // accept many interactions, but we do this to be extra safe.
  UIView* _selectionInteractionBlockingView;
  // The side panel container for the results page.
  LensOverlayPanel* _sidePanel;
  // The bottom sheet container for the results page.
  LensOverlayBottomSheetViewController* _bottomSheet;
  // Layout guide separating the selection UI and results in the side panel.
  UILayoutGuide* _splitViewLayoutGuide;
  // The constraint controlling the display of the side panel.
  NSLayoutConstraint* _splitViewConstraint;
}

- (instancetype)initWithLensOverlayCommandsHandler:
    (id<LensOverlayCommands>)handler {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _overlayCommandsHandler = handler;
    _bottomSheet = [[LensOverlayBottomSheetViewController alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  self.view.accessibilityIdentifier = kLenscontainerViewAccessibilityIdentifier;
  self.view.clipsToBounds = YES;

  if (!self.selectionViewController) {
    return;
  }

  _splitViewLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:_splitViewLayoutGuide];
  _splitViewConstraint = [self.view.trailingAnchor
      constraintEqualToAnchor:_splitViewLayoutGuide.leadingAnchor];
  [NSLayoutConstraint activateConstraints:@[
    _splitViewConstraint,
    [_splitViewLayoutGuide.trailingAnchor
        constraintEqualToAnchor:_splitViewLayoutGuide.leadingAnchor],
    [_splitViewLayoutGuide.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [_splitViewLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  [self addChildViewController:self.selectionViewController];
  [self.view addSubview:self.selectionViewController.view];
  [self.selectionViewController didMoveToParentViewController:self];
  self.selectionViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  AddSameConstraintsToSides(
      self.selectionViewController.view, self.view,
      (LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kBottom));
  [NSLayoutConstraint activateConstraints:@[
    [self.selectionViewController.view.trailingAnchor
        constraintEqualToAnchor:_splitViewLayoutGuide.leadingAnchor],
  ]];

  [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.class ]
                     withAction:@selector(sizeClassDidChange)];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.selectionViewController);
  [self.delegate lensOverlayContainerDidAppear:self animated:animated];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskPortrait;
}

- (BOOL)selectionInteractionDisabled {
  return _selectionInteractionBlockingView != nil;
}

- (BOOL)isSidePanelPresented {
  return _sidePanel != nil;
}

- (BOOL)isSidePanelOpen {
  return _splitViewConstraint.constant != 0;
}

- (void)setSidePanelOpen:(BOOL)sidePanelOpen {
  if (sidePanelOpen) {
    _splitViewConstraint.constant = kSidePanelWidth;
  } else {
    _splitViewConstraint.constant = 0;
  }
}

- (void)setSelectionInteractionDisabled:(BOOL)selectionInteractionDisabled {
  if (!selectionInteractionDisabled) {
    [_selectionInteractionBlockingView removeFromSuperview];
    _selectionInteractionBlockingView = nil;
    return;
  }

  if (_selectionInteractionBlockingView) {
    return;
  }

  UIView* blocker = [[UIView alloc] init];
  blocker.backgroundColor = UIColor.clearColor;
  blocker.userInteractionEnabled = YES;
  [self.view addSubview:blocker];
  blocker.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, blocker);
  _selectionInteractionBlockingView = blocker;
}

- (void)fadeSelectionUIWithDuration:(NSTimeInterval)duration
                         completion:(void (^)())completion {
  [UIView animateWithDuration:duration
      animations:^{
        self.view.backgroundColor = [UIColor clearColor];
        self.selectionViewController.view.alpha = 0;
      }
      completion:^(BOOL success) {
        if (completion) {
          completion();
        }
      }];
}

- (void)presentViewControllerInBottomSheet:(UIViewController*)viewController
                                  animated:(BOOL)animated
                                completion:(ProceduralBlock)completion {
  _bottomSheet.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_bottomSheet];
  [self.view addSubview:_bottomSheet.view];
  [_bottomSheet didMoveToParentViewController:self];

  AddSameConstraints(_bottomSheet.view, self.view);
  [_bottomSheet setContent:viewController];
  [self.view layoutIfNeeded];
  [_bottomSheet presentAnimated:animated completion:completion];
}

- (void)presentViewControllerInSidePanel:(UIViewController*)viewController
                                animated:(BOOL)animated
                              completion:(ProceduralBlock)completion {
  _sidePanel = [[LensOverlayPanel alloc] initWithContent:viewController
                                            insetContent:YES];
  _sidePanel.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_sidePanel];
  [self.view addSubview:_sidePanel.view];
  [_sidePanel didMoveToParentViewController:self];

  AddSameConstraintsToSides(_sidePanel.view, _splitViewLayoutGuide,
                            (LayoutSides::kTop | LayoutSides::kBottom));
  [NSLayoutConstraint activateConstraints:@[
    [_sidePanel.view.leadingAnchor
        constraintEqualToAnchor:_splitViewLayoutGuide.trailingAnchor],
    [_sidePanel.view.widthAnchor constraintEqualToConstant:kSidePanelWidth],
  ]];

  self.selectionViewController.view.clipsToBounds = YES;
  self.selectionViewController.view.layer.maskedCorners =
      [self selectionUICornerMask];

  if (!animated) {
    self.sidePanelOpen = YES;
    self.selectionViewController.view.layer.cornerRadius =
        kSelectionUICornerRadius;
    if (completion) {
      completion();
    }
    return;
  }

  [self.view layoutIfNeeded];
  [UIView animateWithDuration:kSidePannelAnimationDuration
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        self.selectionViewController.view.layer.cornerRadius =
            kSelectionUICornerRadius;
        self.sidePanelOpen = YES;
        [self.selectionViewController
            zoomImageToCenter:UIEdgeInsetsMake(0, kSidePanelSelectionPadding, 0,
                                               kSidePanelSelectionPadding)];
        [self.view layoutIfNeeded];
      }
      completion:^(BOOL) {
        if (completion) {
          completion();
        }
      }];
}

- (void)dismissSidePanelAnimated:(BOOL)animated
                      completion:(ProceduralBlock)completion {
  if (!self.isSidePanelPresented) {
    completion();
    return;
  }

  if (!animated) {
    self.sidePanelOpen = NO;
    self.selectionViewController.view.layer.cornerRadius = 0;
    self.selectionViewController.view.layer.backgroundColor =
        [UIColor clearColor].CGColor;
    [self sidePanelDidDismissAnimated:animated];
    if (completion) {
      completion();
    }
    return;
  }

  [UIView animateWithDuration:kSidePannelAnimationDuration
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        self.sidePanelOpen = NO;
        self.selectionViewController.view.layer.cornerRadius = 0;
        self.selectionViewController.view.layer.backgroundColor =
            [UIColor clearColor].CGColor;
        [self.view layoutIfNeeded];
      }
      completion:^(BOOL) {
        [self sidePanelDidDismissAnimated:animated];
        if (completion) {
          completion();
        }
      }];
}

- (void)dismissBottomSheetAnimated:(BOOL)animated
                        completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [_bottomSheet dismissAnimated:animated
                     completion:^{
                       [weakSelf bottomSheetDidDismissAnimated:animated];
                       if (completion) {
                         completion();
                       }
                     }];
}

// Called after the side panel gets dismissed.
- (void)sidePanelDidDismissAnimated:(BOOL)animated {
  [self.selectionViewController setOcclusionInsets:UIEdgeInsetsZero
                                        reposition:YES
                                          animated:animated];
  [_sidePanel removeFromParentViewController];
  [_sidePanel.view removeFromSuperview];
  _sidePanel = nil;
}

// Called after the side panel gets dismissed.
- (void)bottomSheetDidDismissAnimated:(BOOL)animated {
  [self.selectionViewController setOcclusionInsets:UIEdgeInsetsZero
                                        reposition:YES
                                          animated:animated];
  [_bottomSheet removeFromParentViewController];
  [_bottomSheet.view removeFromSuperview];
}

- (void)sizeClassDidChange {
  [self.delegate lensOverlayContainerDidChangeSizeClass:self];
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self closeOverlayRequested];
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

- (void)closeOverlayRequested {
  [_overlayCommandsHandler destroyLensUI:YES
                                  reason:lens::LensOverlayDismissalSource::
                                             kAccessibilityEscapeGesture];
}

- (void)escapeButtonPressed {
  [self closeOverlayRequested];
}

#pragma mark - Private
- (CACornerMask)selectionUICornerMask {
  if (base::i18n::IsRTL()) {
    return kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner;
  } else {
    return kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
  }
}

@end
