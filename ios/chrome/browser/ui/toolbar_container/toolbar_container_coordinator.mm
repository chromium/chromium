// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_coordinator.h"

#include <memory>

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view_controller.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarContainerCoordinator () {
  // The updater for the container view controller.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}
// The container view controller.
@property(nonatomic, strong)
    ToolbarContainerViewController* containerViewController;
// Whether the coordinator's UI has been started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The container's type.
@property(nonatomic, assign) ToolbarContainerType type;
@end

@implementation ToolbarContainerCoordinator
@synthesize containerViewController = _containerViewController;
@synthesize toolbarCoordinators = _toolbarCoordinators;
@synthesize type = _type;
@synthesize started = _started;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                type:(ToolbarContainerType)type {
  if (self = [super initWithBaseViewController:nil browserState:browserState]) {
    _type = type;
  }
  return self;
}

#pragma mark - Accessors

- (UIViewController*)viewController {
  return self.containerViewController;
}

- (void)setToolbarCoordinators:
    (NSArray<ChromeCoordinator*>*)toolbarCoordinators {
  if ([_toolbarCoordinators isEqualToArray:toolbarCoordinators])
    return;

  if (self.started)
    [self stopToolbarCoordinators];
  _toolbarCoordinators = toolbarCoordinators;
  if (self.started)
    [self startToolbarCoordinators];
}

#pragma mark - Public

- (CGFloat)toolbarStackHeightForFullscreenProgress:(CGFloat)progress {
  if (!self.started)
    return 0.0;
  const toolbar_container::HeightRange& stackHeightRange =
      self.containerViewController.heightRange;
  return stackHeightRange.GetInterpolatedHeight(progress);
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  [super start];
  // Create the container view controller.
  self.containerViewController = [[ToolbarContainerViewController alloc] init];
  BOOL isPrimary = self.type == ToolbarContainerType::kPrimary;
  self.containerViewController.orientation =
      isPrimary ? ToolbarContainerOrientation::kTopToBottom
                : ToolbarContainerOrientation::kBottomToTop;
  self.containerViewController.collapsesSafeArea = !isPrimary;
  [self startToolbarCoordinators];
  // Start observing fullscreen events.
  _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      FullscreenControllerFactory::GetForBrowserState(self.browserState),
      self.containerViewController);
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.containerViewController willMoveToParentViewController:nil];
  [self.containerViewController.view removeFromSuperview];
  [self.containerViewController removeFromParentViewController];
  self.containerViewController = nil;
  [self stopToolbarCoordinators];
  _fullscreenUIUpdater = nullptr;
  self.started = NO;
}

#pragma mark - Private

// Returns the view controllers associated with the toobar coordinators.
- (NSArray<UIViewController*>*)toolbarViewControllers {
  NSMutableArray<UIViewController*>* toolbarViewControllers =
      [[NSMutableArray alloc] init];
  for (ChromeCoordinator* coordinator in _toolbarCoordinators) {
    if ([coordinator respondsToSelector:@selector(viewController)]) {
      id toolbarCoordinator = coordinator;
      [toolbarViewControllers addObject:[toolbarCoordinator viewController]];
    }
  }
  return toolbarViewControllers;
}

// Starts the toolbar coordinators and adds their view to the container.
- (void)startToolbarCoordinators {
  for (ChromeCoordinator* coordinator in _toolbarCoordinators) {
    [coordinator start];
  }
  self.containerViewController.toolbars = [self toolbarViewControllers];
}

// Stops the toolbar coordinators and removes their views from the container.
- (void)stopToolbarCoordinators {
  self.containerViewController.toolbars = nil;
  for (ChromeCoordinator* coordinator in _toolbarCoordinators) {
    [coordinator stop];
  }
}

@end
