// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"

#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_mutator.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tabs_closure_animation.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ReaderModeViewController ()

@end

@implementation ReaderModeViewController {
  UIView* _contentView;
  TabsClosureAnimation* _tabsClosureAnimation;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.accessibilityIdentifier = kReaderModeViewAccessibilityIdentifier;

  if (@available(iOS 17.0, *)) {
    __weak __typeof(self) weakSelf = self;
    id handler = ^(id<UITraitEnvironment> traitEnvironment,
                   UITraitCollection* previousCollection) {
      [weakSelf updateTheme];
    };
    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                      withHandler:handler];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateTheme];
}

#pragma mark - Public

- (void)moveToParentViewController:(UIViewController*)parent
                          animated:(BOOL)animated {
  [self willMoveToParentViewController:parent];
  [parent addChildViewController:self];
  [parent.view addSubview:self.view];
  AddSameConstraints(
      [NamedGuide guideWithName:kContentAreaGuide view:parent.view], self.view);
  if (animated) {
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
    _tabsClosureAnimation =
        [[TabsClosureAnimation alloc] initWithWindow:self.view
                                           gridCells:@[ _contentView ]];
    _tabsClosureAnimation.type = TabsClosureAnimationType::kRevealGridCells;
    _tabsClosureAnimation.startPoint = CGPointMake(0.5, 0);
    self.view.userInteractionEnabled = NO;
    __weak __typeof(self) weakSelf = self;
    [_tabsClosureAnimation animateWithCompletion:^{
      [weakSelf tabsClosureAnimationDidComplete];
    }];
  } else {
    [self didMoveToParentViewController:parent];
  }
}

- (void)removeFromParentViewControllerAnimated:(BOOL)animated {
  [self willMoveToParentViewController:nil];
  if (animated) {
    _tabsClosureAnimation =
        [[TabsClosureAnimation alloc] initWithWindow:self.view
                                           gridCells:@[ _contentView ]];
    _tabsClosureAnimation.startPoint = CGPointMake(0.5, 0);
    self.view.userInteractionEnabled = NO;
    __weak __typeof(self) weakSelf = self;
    [_tabsClosureAnimation animateWithCompletion:^{
      [weakSelf tabsClosureAnimationDidComplete];
    }];
  } else {
    [self.view removeFromSuperview];
    [self removeFromParentViewController];
    [self didMoveToParentViewController:nil];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17.0, *)) {
    return;
  }
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self updateTheme];
  }
}
#endif

#pragma mark - ReaderModeConsumer

- (void)setContentView:(UIView*)contentView {
  if (_contentView) {
    _contentView.hidden = YES;
    [_contentView removeFromSuperview];
  }
  _contentView = contentView;
  if (_contentView) {
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_contentView];
    _contentView.hidden = NO;
    AddSameConstraints(self.view, _contentView);
  }
}

#pragma mark - Private

- (void)updateTheme {
  if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
    [self.mutator setDefaultTheme:dom_distiller::mojom::Theme::kDark];
  } else {
    [self.mutator setDefaultTheme:dom_distiller::mojom::Theme::kLight];
  }
}

// First restores user interaction in `self.view`. In case of dismissal, removes
// the view and view controller from their hierarchy. Then calls
// `didMoveToParentViewController:` and frees `_tabsClosureAnimation`.
- (void)tabsClosureAnimationDidComplete {
  self.view.userInteractionEnabled = YES;
  if (_tabsClosureAnimation.type == TabsClosureAnimationType::kHideGridCells) {
    [self.view removeFromSuperview];
    [self removeFromParentViewController];
  }
  [self didMoveToParentViewController:self.parentViewController];
  _tabsClosureAnimation = nil;
}

@end
