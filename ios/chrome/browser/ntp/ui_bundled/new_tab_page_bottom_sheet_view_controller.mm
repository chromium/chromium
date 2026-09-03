// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import <cmath>

#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Snapping states for the bottom sheet.
typedef NS_ENUM(NSInteger, BottomSheetSnappingState) {
  BottomSheetSnappingStateCollapsed,
  BottomSheetSnappingStateResting,
  BottomSheetSnappingStateExpanded,
};

// Spacing/margin constants for content container.
constexpr CGFloat kContentContainerTopMargin = 16.0;
constexpr CGFloat kMagicStackToFeedSpacing = 16.0;

// Minimum drag velocity required to trigger a state transition.
constexpr CGFloat kMinimumDragVelocityToChangeState = 250.0;

}  // namespace

@interface NewTabPageBottomSheetViewController () <UIGestureRecognizerDelegate,
                                                   UIScrollViewDelegate>
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetTopConstraint;
@end

@implementation NewTabPageBottomSheetViewController {
  UIVisualEffectView* _blurBackgroundView;
  UIView* _dragHandle;
  UIView* _headerContainerView;
  UIView* _magicStackContainerView;
  UIView* _mostVisitedContainerView;
  UIView* _mostVisitedView;
  UIView* _contentContainerView;
  NTPCardBackgroundView* _feedCardBackgroundView;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;
  CGFloat _lastFeedPanTranslationY;

  UIPanGestureRecognizer* _sheetPanGesture;
  __weak UIScrollView* _feedScrollView;

  // Proxy to intercept feed scroll events for VoiceOver auto-expand.
  ScrollDelegateProxy* _scrollProxy;

  // The original delegate of the feed scroll view.
  __weak id<UIScrollViewDelegate> _originalFeedDelegate;

  BOOL _isBottomOmnibox;
  NSArray<NSLayoutConstraint*>* _headerContainerConstraints;
  NSArray<NSLayoutConstraint*>* _feedCardBackgroundConstraints;
  NSLayoutConstraint* _magicStackTopConstraint;
}

#pragma mark - Public

- (CGFloat)headerHeight {
  CGFloat height = kMagicStackHeight + kMagicStackToFeedSpacing;
  if (IsMVTInBottomSheetEnabled() && _mostVisitedContainerView) {
    CGFloat mvtHeight =
        MostVisitedContainerHeight(_mostVisitedContainerView, _mostVisitedView);
    if (mvtHeight > 0) {
      height += mvtHeight +
                content_suggestions::ReducedModuleSpacing(self.traitCollection);
    }
  }
  return height;
}

- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox {
  _isBottomOmnibox = isBottomOmnibox;
  [self updateFeedInsets];
}

- (void)updateFeedInsets {
  if (!_feedScrollView) {
    return;
  }
  CGFloat topInset = [self headerHeight];
  CGFloat bottomInset = 0.0;
  if (_isBottomOmnibox && _sheetState == BottomSheetSnappingStateExpanded) {
    bottomInset = kToolbarHeight + self.view.safeAreaInsets.bottom;
  }
  UIEdgeInsets insets = UIEdgeInsetsMake(topInset, 0, bottomInset, 0);
  if (!UIEdgeInsetsEqualToEdgeInsets(_feedScrollView.contentInset, insets)) {
    _feedScrollView.contentInset = insets;
    _feedScrollView.verticalScrollIndicatorInsets = insets;
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(voiceOverStatusDidChange)
             name:UIAccessibilityVoiceOverStatusDidChangeNotification
           object:nil];

  _sheetState = BottomSheetSnappingStateResting;

  self.view.layer.cornerRadius = 24.0;
  self.view.layer.masksToBounds = YES;

  _blurBackgroundView = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect
                         effectWithStyle:UIBlurEffectStyleSystemMaterial]];
  _blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _blurBackgroundView.userInteractionEnabled = NO;
  [self.view insertSubview:_blurBackgroundView atIndex:0];
  AddSameConstraints(self.view, _blurBackgroundView);

  // Add drag handle to bottom sheet.
  _dragHandle = [[UIView alloc] init];
  _dragHandle.translatesAutoresizingMaskIntoConstraints = NO;
  _dragHandle.backgroundColor = [UIColor colorNamed:kTextTertiaryColor];
  _dragHandle.layer.cornerRadius = 2.5;
  [self.view addSubview:_dragHandle];

  [NSLayoutConstraint activateConstraints:@[
    [_dragHandle.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [_dragHandle.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:8],
    [_dragHandle.widthAnchor constraintEqualToConstant:36],
    [_dragHandle.heightAnchor constraintEqualToConstant:5],
  ]];

  // Add content container view that fills the bottom sheet below the drag
  // handle.
  _contentContainerView = [[UIView alloc] init];
  _contentContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [_contentContainerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_contentContainerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_contentContainerView.topAnchor
        constraintEqualToAnchor:_dragHandle.bottomAnchor
                       constant:kContentContainerTopMargin],
    [_contentContainerView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // Add header container view that encapsulates MVT and Magic Stack.
  _headerContainerView = [[UIView alloc] init];
  _headerContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add most visited tiles container view if feature is enabled.
  if (IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView = [[UIView alloc] init];
    _mostVisitedContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [_headerContainerView addSubview:_mostVisitedContainerView];

    [NSLayoutConstraint activateConstraints:@[
      [_mostVisitedContainerView.leadingAnchor
          constraintEqualToAnchor:_headerContainerView.leadingAnchor],
      [_mostVisitedContainerView.trailingAnchor
          constraintEqualToAnchor:_headerContainerView.trailingAnchor],
      [_mostVisitedContainerView.topAnchor
          constraintEqualToAnchor:_headerContainerView.topAnchor],
    ]];
  }

  // Add magic stack container view.
  _magicStackContainerView = [[UIView alloc] init];
  _magicStackContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_headerContainerView addSubview:_magicStackContainerView];

  if (IsMVTInBottomSheetEnabled()) {
    _magicStackTopConstraint = [_magicStackContainerView.topAnchor
        constraintEqualToAnchor:_mostVisitedContainerView.bottomAnchor
                       constant:content_suggestions::ReducedModuleSpacing(
                                    self.traitCollection)];
  } else {
    _magicStackTopConstraint = [_magicStackContainerView.topAnchor
        constraintEqualToAnchor:_headerContainerView.topAnchor
                       constant:0.0];
  }
  _magicStackTopConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_magicStackContainerView.leadingAnchor
        constraintEqualToAnchor:_headerContainerView.leadingAnchor],
    [_magicStackContainerView.trailingAnchor
        constraintEqualToAnchor:_headerContainerView.trailingAnchor],
    [_magicStackContainerView.heightAnchor
        constraintEqualToConstant:kMagicStackHeight],
    [_headerContainerView.bottomAnchor
        constraintEqualToAnchor:_magicStackContainerView.bottomAnchor],
  ]];

  // Add feed card background view.
  _feedCardBackgroundView = [[NTPCardBackgroundView alloc] init];
  _feedCardBackgroundView.userInteractionEnabled = NO;
  _feedCardBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _feedCardBackgroundView.layer.cornerRadius = kHomeModuleContainerCornerRadius;
  _feedCardBackgroundView.layer.maskedCorners =
      kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
  _feedCardBackgroundView.clipsToBounds = YES;
  _feedCardBackgroundView.layer.zPosition = -CGFLOAT_MAX;

  // Add pan gesture recognizer.
  _sheetPanGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  _sheetPanGesture.delegate = self;
  [self.view addGestureRecognizer:_sheetPanGesture];

  if (_feedViewController) {
    [self embedFeedViewController];
  } else {
    [self updateHeaderContainerHierarchy];
  }

  if (_mostVisitedView) {
    [self embedMostVisitedView:_mostVisitedView];
  }

  if (_magicStackViewController) {
    [self embedMagicStackViewController];
  }

  __weak __typeof(self) weakSelf = self;
  [self registerForTraitChanges:@[
    UITraitHorizontalSizeClass.class,
    UITraitVerticalSizeClass.class,
    UITraitPreferredContentSizeCategory.class,
    NewTabPageImageBackgroundTrait.class,
  ]
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf handleTraitChanges];
                    }];
  [self applyBackgroundTheme];

  [self updateContentContainerInsetForOffset:
            [self targetOffsetForState:_sheetState]];
}

- (void)handleTraitChanges {
  if (IsMVTInBottomSheetEnabled() && _magicStackTopConstraint) {
    _magicStackTopConstraint.constant =
        content_suggestions::ReducedModuleSpacing(self.traitCollection);
  }
  [self applyBackgroundTheme];
  [self updateFeedInsets];
  [self updateBottomSheetPositionAnimated:NO];
}

- (void)applyBackgroundTheme {
  BOOL hasBlurredBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];
  if (hasBlurredBackground) {
    self.view.backgroundColor = UIColor.clearColor;
    _blurBackgroundView.hidden = NO;
  } else {
    self.view.backgroundColor = [UIColor colorNamed:kSurfaceContainerLowColor];
    _blurBackgroundView.hidden = YES;
  }
}

- (void)updateHeaderContainerHierarchy {
  if (!self.isViewLoaded || !_headerContainerView || !_contentContainerView) {
    return;
  }

  // Pre-Detach Magic Stack child view controller before reparenting
  // its container view.
  [self detachMagicStackViewController];

  // Pure view move and constraint updates.
  [NSLayoutConstraint deactivateConstraints:_headerContainerConstraints];
  [NSLayoutConstraint deactivateConstraints:_feedCardBackgroundConstraints];
  if (_feedScrollView &&
      [_feedScrollView isDescendantOfView:_contentContainerView]) {
    if (_headerContainerView.superview != _feedScrollView) {
      [_headerContainerView removeFromSuperview];
      [_feedScrollView addSubview:_headerContainerView];
    }
    _headerContainerConstraints = @[
      [_headerContainerView.leadingAnchor
          constraintEqualToAnchor:_contentContainerView.leadingAnchor],
      [_headerContainerView.trailingAnchor
          constraintEqualToAnchor:_contentContainerView.trailingAnchor],
      [_headerContainerView.bottomAnchor
          constraintEqualToAnchor:_feedScrollView.topAnchor
                         constant:-kMagicStackToFeedSpacing],
    ];

    if (_feedCardBackgroundView.superview != _feedScrollView) {
      [_feedCardBackgroundView removeFromSuperview];
      [_feedScrollView insertSubview:_feedCardBackgroundView atIndex:0];
    }
    [_feedScrollView sendSubviewToBack:_feedCardBackgroundView];
    _feedCardBackgroundConstraints = @[
      [_feedCardBackgroundView.topAnchor
          constraintEqualToAnchor:_feedScrollView.topAnchor],
      [_feedCardBackgroundView.leadingAnchor
          constraintEqualToAnchor:_contentContainerView.leadingAnchor],
      [_feedCardBackgroundView.trailingAnchor
          constraintEqualToAnchor:_contentContainerView.trailingAnchor],
      [_feedCardBackgroundView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_contentContainerView
                                                   .heightAnchor],
    ];
  } else {
    if (_headerContainerView.superview != _contentContainerView) {
      [_headerContainerView removeFromSuperview];
      [_contentContainerView addSubview:_headerContainerView];
    }
    _headerContainerConstraints = @[
      [_headerContainerView.leadingAnchor
          constraintEqualToAnchor:_contentContainerView.leadingAnchor],
      [_headerContainerView.trailingAnchor
          constraintEqualToAnchor:_contentContainerView.trailingAnchor],
      [_headerContainerView.topAnchor
          constraintEqualToAnchor:_contentContainerView.topAnchor],
    ];

    if (_feedCardBackgroundView.superview != _contentContainerView) {
      [_feedCardBackgroundView removeFromSuperview];
      [_contentContainerView insertSubview:_feedCardBackgroundView atIndex:0];
    }
    _feedCardBackgroundConstraints = @[
      [_feedCardBackgroundView.topAnchor
          constraintEqualToAnchor:_headerContainerView.bottomAnchor
                         constant:kMagicStackToFeedSpacing],
      [_feedCardBackgroundView.leadingAnchor
          constraintEqualToAnchor:_contentContainerView.leadingAnchor],
      [_feedCardBackgroundView.trailingAnchor
          constraintEqualToAnchor:_contentContainerView.trailingAnchor],
      [_feedCardBackgroundView.bottomAnchor
          constraintEqualToAnchor:_contentContainerView.bottomAnchor],
    ];
  }
  [NSLayoutConstraint activateConstraints:_headerContainerConstraints];
  [NSLayoutConstraint activateConstraints:_feedCardBackgroundConstraints];
  [self updateFeedInsets];

  // Post-Attach Magic Stack to target parent (feed VC if inside
  // feed scroll view, self otherwise).
  [self embedMagicStackViewController];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [self setupSuperviewConstraints];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateFeedInsets];
  if (_feedScrollView && _feedCardBackgroundView &&
      _feedCardBackgroundView.superview == _feedScrollView) {
    [_feedScrollView sendSubviewToBack:_feedCardBackgroundView];
  }
  if (self.view.superview &&
      !CGSizeEqualToSize(_lastSize, self.view.superview.bounds.size)) {
    _lastSize = self.view.superview.bounds.size;
    [self updateBottomSheetPositionAnimated:NO];
  }
}

- (void)invalidate {
  [self detachMagicStackViewController];
  [self detachFeedViewController];
  _magicStackViewController = nil;
  _feedViewController = nil;
  self.delegate = nil;
  _mostVisitedView = nil;
  _headerContainerView = nil;
  _feedCardBackgroundView = nil;
  _headerContainerConstraints = nil;
  _feedCardBackgroundConstraints = nil;
  _magicStackTopConstraint = nil;
}

- (BOOL)accessibilityPerformEscape {
  if (_sheetState == BottomSheetSnappingStateExpanded) {
    _sheetState = BottomSheetSnappingStateResting;
    [self updateBottomSheetPositionAnimated:YES];
    if ([self.delegate
            respondsToSelector:@selector(
                                   bottomSheetViewControllerDidEscape:)]) {
      [self.delegate bottomSheetViewControllerDidEscape:self];
    }
    return YES;
  }
  return NO;
}

#pragma mark - Feed Integration

- (UIScrollView*)findScrollViewInView:(UIView*)view {
  if ([view isKindOfClass:[UIScrollView class]]) {
    return (UIScrollView*)view;
  }
  for (UIView* subview in view.subviews) {
    UIScrollView* scrollView = [self findScrollViewInView:subview];
    if (scrollView) {
      return scrollView;
    }
  }
  return nil;
}

- (void)resetFeedScrollView {
  if (!_feedScrollView) {
    return;
  }
  _feedScrollView.contentInset = UIEdgeInsetsZero;
  _feedScrollView.verticalScrollIndicatorInsets = UIEdgeInsetsZero;
  [_feedScrollView.panGestureRecognizer removeTarget:self
                                              action:@selector(handleFeedPan:)];
  if (_scrollProxy) {
    _feedScrollView.delegate = _originalFeedDelegate;
    _scrollProxy = nil;
  }
  _originalFeedDelegate = nil;
  _feedScrollView = nil;
}

- (void)updateFeedScrollViewReference {
  UIScrollView* newFeedScrollView = nil;
  if (_feedViewController) {
    newFeedScrollView = [self findScrollViewInView:_feedViewController.view];
  }

  if (_feedScrollView == newFeedScrollView) {
    return;
  }

  [self resetFeedScrollView];

  _feedScrollView = newFeedScrollView;

  if (_feedScrollView) {
    _originalFeedDelegate = _feedScrollView.delegate;
    if ([self isVoiceOverRunning]) {
      _scrollProxy = [[ScrollDelegateProxy alloc]
          initWithInterceptingTarget:self
                      originalTarget:_originalFeedDelegate];
      _feedScrollView.delegate = _scrollProxy;
    }

    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        [self isVoiceOverRunning];
    [_feedScrollView.panGestureRecognizer addTarget:self
                                             action:@selector(handleFeedPan:)];
  }
}

- (void)detachFeedViewController {
  [self resetFeedScrollView];
  if (_feedViewController) {
    [_feedViewController willMoveToParentViewController:nil];
    [_feedViewController.view removeFromSuperview];
    [_feedViewController removeFromParentViewController];
  }
}

- (void)setFeedViewController:(UIViewController*)feedViewController {
  if (_feedViewController == feedViewController) {
    return;
  }
  [self detachFeedViewController];
  _feedViewController = feedViewController;
  if (self.isViewLoaded) {
    if (_feedViewController) {
      [self embedFeedViewController];
    } else {
      [self updateHeaderContainerHierarchy];
    }
  }
}

- (void)embedFeedViewController {
  if (!_feedViewController || !_contentContainerView || !self.isViewLoaded) {
    return;
  }
  if (_feedViewController.parentViewController == self &&
      [_feedViewController.view isDescendantOfView:_contentContainerView]) {
    return;
  }
  if (_feedViewController.parentViewController) {
    [_feedViewController willMoveToParentViewController:nil];
    [_feedViewController.view removeFromSuperview];
    [_feedViewController removeFromParentViewController];
  }
  [self addChildViewController:_feedViewController];
  _feedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentContainerView addSubview:_feedViewController.view];
  AddSameConstraints(_feedViewController.view, _contentContainerView);
  if (_feedCardBackgroundView) {
    [_contentContainerView sendSubviewToBack:_feedCardBackgroundView];
  }
  [_feedViewController didMoveToParentViewController:self];

  _feedViewController.view.hidden = NO;
  [self updateFeedScrollViewReference];
  [self updateHeaderContainerHierarchy];
}

- (void)detachMagicStackViewController {
  if (!_magicStackViewController) {
    return;
  }
  [_magicStackViewController willMoveToParentViewController:nil];
  [_magicStackViewController.view removeFromSuperview];
  [_magicStackViewController removeFromParentViewController];
}

- (void)setMagicStackViewController:
    (UIViewController*)magicStackViewController {
  if (_magicStackViewController == magicStackViewController) {
    return;
  }
  [self detachMagicStackViewController];
  _magicStackViewController = magicStackViewController;
  if (self.isViewLoaded && _magicStackViewController) {
    [self embedMagicStackViewController];
  }
}

- (void)embedMagicStackViewController {
  if (!_magicStackViewController || !_magicStackContainerView ||
      !self.isViewLoaded) {
    return;
  }
  UIViewController* targetParent =
      (_feedScrollView &&
       [_feedScrollView isDescendantOfView:_contentContainerView] &&
       _feedViewController)
          ? _feedViewController
          : self;
  if (_magicStackViewController.parentViewController == targetParent &&
      _magicStackViewController.view.superview == _magicStackContainerView) {
    return;
  }
  if (_magicStackViewController.parentViewController) {
    [_magicStackViewController willMoveToParentViewController:nil];
    [_magicStackViewController.view removeFromSuperview];
    [_magicStackViewController removeFromParentViewController];
  }
  [targetParent addChildViewController:_magicStackViewController];
  _magicStackViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [_magicStackContainerView addSubview:_magicStackViewController.view];
  AddSameConstraints(_magicStackViewController.view, _magicStackContainerView);
  [_magicStackViewController didMoveToParentViewController:targetParent];
}

- (void)embedMostVisitedView:(UIView*)mostVisitedView {
  if (_mostVisitedView != mostVisitedView) {
    if (_mostVisitedView) {
      [_mostVisitedView removeFromSuperview];
    }
    _mostVisitedView = mostVisitedView;
  }
  if (self.isViewLoaded && _mostVisitedView && _mostVisitedContainerView) {
    if (_mostVisitedView.superview == _mostVisitedContainerView) {
      return;
    }
    _mostVisitedView.translatesAutoresizingMaskIntoConstraints = NO;
    [_mostVisitedContainerView addSubview:_mostVisitedView];
    AddSameConstraints(_mostVisitedView, _mostVisitedContainerView);
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
    [self updateFeedInsets];
    CGFloat offset = _bottomSheetTopConstraint
                         ? _bottomSheetTopConstraint.constant
                         : [self restingOffset];
    [self updateContentContainerInsetForOffset:offset];
  }
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  return [self.delegate collapsedOffsetForBottomSheetViewController:self];
}

- (CGFloat)restingOffset {
  return [self.delegate restingOffsetForBottomSheetViewController:self];
}

- (CGFloat)expandedOffset {
  return [self.delegate expandedOffsetForBottomSheetViewController:self];
}

- (CGFloat)targetOffsetForState:(BottomSheetSnappingState)state {
  switch (state) {
    case BottomSheetSnappingStateCollapsed:
      return [self collapsedOffset];
    case BottomSheetSnappingStateResting:
      return [self restingOffset];
    case BottomSheetSnappingStateExpanded:
      return [self expandedOffset];
  }
}

#pragma mark - Bottom Sheet Snapping and Panning

- (void)setupSuperviewConstraints {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor],
    [self.view.trailingAnchor constraintEqualToAnchor:superview.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:superview.bottomAnchor],
  ]];

  if (!_bottomSheetTopConstraint) {
    _bottomSheetTopConstraint =
        [self.view.topAnchor constraintEqualToAnchor:superview.topAnchor
                                            constant:[self restingOffset]];
    _bottomSheetTopConstraint.active = YES;
  }
}

- (void)updateBottomSheetPositionAnimated:(BOOL)animated {
  if (!_bottomSheetTopConstraint) {
    return;
  }
  CGFloat targetConstant = [self targetOffsetForState:_sheetState];

  if (_feedScrollView) {
    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        UIAccessibilityIsVoiceOverRunning();
    _feedScrollView.bounces = (_sheetState == BottomSheetSnappingStateExpanded);
    [self updateFeedInsets];
  }

  if (_sheetState != BottomSheetSnappingStateExpanded && _feedScrollView) {
    CGFloat topInset = _feedScrollView.contentInset.top;
    [_feedScrollView setContentOffset:CGPointMake(0, -topInset)
                             animated:animated];
  }

  if (!animated) {
    _bottomSheetTopConstraint.constant = targetConstant;
    [self updateContentContainerInsetForOffset:targetConstant];
    [self.delegate bottomSheetViewController:self
                          didUpdateTopOffset:targetConstant];
  } else {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.3
                          delay:0
         usingSpringWithDamping:0.85
          initialSpringVelocity:0.5
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       NewTabPageBottomSheetViewController* strongSelf =
                           weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       strongSelf.bottomSheetTopConstraint.constant =
                           targetConstant;
                       [strongSelf
                           updateContentContainerInsetForOffset:targetConstant];
                       [strongSelf.delegate
                           bottomSheetViewController:strongSelf
                                  didUpdateTopOffset:targetConstant];
                       [strongSelf.view.superview layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)snapSheetWithVelocity:(CGPoint)velocity
              currentConstant:(CGFloat)currentConstant {
  CGFloat collapsed = [self collapsedOffset];
  CGFloat resting = [self restingOffset];
  CGFloat expanded = [self expandedOffset];

  BottomSheetSnappingState targetState = _sheetState;

  if (std::abs(velocity.y) > kMinimumDragVelocityToChangeState) {
    if (velocity.y > 0) {
      // Swiping down: transition to the next lower state.
      if (_sheetState == BottomSheetSnappingStateExpanded) {
        targetState = BottomSheetSnappingStateResting;
      } else if (_sheetState == BottomSheetSnappingStateResting) {
        targetState = BottomSheetSnappingStateCollapsed;
      }
    } else {
      // Swiping up: transition to the next higher state.
      if (_sheetState == BottomSheetSnappingStateCollapsed) {
        targetState = BottomSheetSnappingStateResting;
      } else if (_sheetState == BottomSheetSnappingStateResting) {
        targetState = BottomSheetSnappingStateExpanded;
      }
    }
  } else {
    // Slow drag: snap to the closest state based on distance from
    // currentConstant.
    CGFloat distExpanded = std::abs(currentConstant - expanded);
    CGFloat distResting = std::abs(currentConstant - resting);
    CGFloat distCollapsed = std::abs(currentConstant - collapsed);

    CGFloat minDist = MIN(distExpanded, MIN(distResting, distCollapsed));
    if (minDist == distExpanded) {
      targetState = BottomSheetSnappingStateExpanded;
    } else if (minDist == distResting) {
      targetState = BottomSheetSnappingStateResting;
    } else {
      targetState = BottomSheetSnappingStateCollapsed;
    }
  }

  _sheetState = targetState;
  [self updateBottomSheetPositionAnimated:YES];
}

- (void)updateContentContainerInsetForOffset:(CGFloat)topOffset {
  _magicStackContainerView.alpha = 1.0;
  if (_mostVisitedContainerView) {
    _mostVisitedContainerView.alpha = 1.0;
  }
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  CGPoint translation = [gesture translationInView:superview];
  CGPoint velocity = [gesture velocityInView:superview];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstant = _bottomSheetTopConstraint.constant;
  }

  if (_sheetState == BottomSheetSnappingStateExpanded && _feedScrollView) {
    CGFloat topInset = _feedScrollView.contentInset.top;
    if (_feedScrollView.contentOffset.y > -topInset) {
      _initialConstant = [self expandedOffset];
      [gesture setTranslation:CGPointZero inView:superview];
      return;
    }
    if (translation.y < 0) {
      _initialConstant = [self expandedOffset];
      [gesture setTranslation:CGPointZero inView:superview];
      return;
    }
  }

  CGFloat targetConstant = _initialConstant + translation.y;
  CGFloat minOffset = [self expandedOffset];
  CGFloat maxOffset = [self collapsedOffset];

  if (targetConstant < minOffset) {
    targetConstant = minOffset;
  } else if (targetConstant > maxOffset) {
    targetConstant = maxOffset;
  }

  if (targetConstant > minOffset && _feedScrollView &&
      _feedScrollView.contentOffset.y > -_feedScrollView.contentInset.top) {
    [_feedScrollView
        setContentOffset:CGPointMake(0, -_feedScrollView.contentInset.top)
                animated:NO];
  }

  _bottomSheetTopConstraint.constant = targetConstant;
  [self updateContentContainerInsetForOffset:targetConstant];
  [self.delegate bottomSheetViewController:self
                        didUpdateTopOffset:targetConstant];

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self snapSheetWithVelocity:velocity currentConstant:targetConstant];
  } else if (gesture.state == UIGestureRecognizerStateCancelled) {
    [self updateBottomSheetPositionAnimated:YES];
  }
}
#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  if (gestureRecognizer == _sheetPanGesture) {
    if (_feedScrollView && _sheetState == BottomSheetSnappingStateExpanded) {
      CGPoint feedPoint = [touch locationInView:_feedScrollView];
      if ([_feedScrollView pointInside:feedPoint withEvent:nil]) {
        return NO;
      }
    }
  }
  return YES;
}

- (void)handleFeedPan:(UIPanGestureRecognizer*)gesture {
  if (_sheetState != BottomSheetSnappingStateExpanded || !_feedScrollView) {
    return;
  }

  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  CGPoint translation = [gesture translationInView:superview];
  CGPoint velocity = [gesture velocityInView:superview];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _lastFeedPanTranslationY = translation.y;
  }

  CGFloat deltaY = translation.y - _lastFeedPanTranslationY;
  _lastFeedPanTranslationY = translation.y;

  CGFloat expandedOffset = [self expandedOffset];
  CGFloat topInset = _feedScrollView.contentInset.top;

  // If the sheet is docked at the top (expandedOffset) and either the feed is
  // scrolled down (contentOffset.y > -topInset) or the user is scrolling
  // further into feed content (deltaY <= 0), allow UIScrollView to handle
  // scrolling natively without modifying sheet position.
  if (_bottomSheetTopConstraint.constant <= expandedOffset) {
    if (_feedScrollView.contentOffset.y > -topInset || deltaY <= 0) {
      _feedScrollView.bounces = YES;
      return;
    }
  }

  // Feed is at the top (contentOffset.y <= -topInset) and user is pulling down
  // (deltaY > 0), or the sheet is already pulled down
  // (_bottomSheetTopConstraint.constant > expandedOffset).
  _feedScrollView.contentOffset = CGPointMake(0, -topInset);
  _feedScrollView.bounces = NO;

  CGFloat maxOffset = [self collapsedOffset];
  CGFloat targetConstant = _bottomSheetTopConstraint.constant + deltaY;
  if (targetConstant < expandedOffset) {
    targetConstant = expandedOffset;
  } else if (targetConstant > maxOffset) {
    targetConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = targetConstant;
  [self updateContentContainerInsetForOffset:targetConstant];
  [self.delegate bottomSheetViewController:self
                        didUpdateTopOffset:targetConstant];

  if (gesture.state == UIGestureRecognizerStateEnded) {
    if (_bottomSheetTopConstraint.constant > expandedOffset) {
      [self snapSheetWithVelocity:velocity
                  currentConstant:_bottomSheetTopConstraint.constant];
    } else {
      _feedScrollView.bounces = YES;
    }
  } else if (gesture.state == UIGestureRecognizerStateCancelled) {
    [self updateBottomSheetPositionAnimated:YES];
  }
}

- (void)voiceOverStatusDidChange {
  if (_feedScrollView) {
    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        [self isVoiceOverRunning];

    if ([self isVoiceOverRunning]) {
      if (!_scrollProxy) {
        _scrollProxy = [[ScrollDelegateProxy alloc]
            initWithInterceptingTarget:self
                        originalTarget:_originalFeedDelegate];
        _feedScrollView.delegate = _scrollProxy;
      }
    } else {
      if (_scrollProxy) {
        _feedScrollView.delegate = _originalFeedDelegate;
        _scrollProxy = nil;
      }
    }
  }
}

- (BOOL)isVoiceOverRunning {
  return UIAccessibilityIsVoiceOverRunning();
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if ([self isVoiceOverRunning]) {
    if (_sheetState != BottomSheetSnappingStateExpanded &&
        scrollView.contentOffset.y > 0) {
      _sheetState = BottomSheetSnappingStateExpanded;
      [self updateBottomSheetPositionAnimated:YES];
    } else if (_sheetState == BottomSheetSnappingStateExpanded &&
               scrollView.contentOffset.y < 0) {
      _sheetState = BottomSheetSnappingStateResting;
      [self updateBottomSheetPositionAnimated:YES];
    }
  }
}

@end
