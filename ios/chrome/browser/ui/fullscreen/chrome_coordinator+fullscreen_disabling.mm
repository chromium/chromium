// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/chrome_coordinator+fullscreen_disabling.h"

#import <objc/runtime.h>

#import <memory>

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"

namespace {
// The key under which ScopedFullscreenDisablerWrapper are associated with their
// ChromeCoordinators.
const void* const kFullscreenDisablerKey = &kFullscreenDisablerKey;
}

#pragma mark - ScopedFullscreenDisablerWrapper

// A wrapper object for ScopedFullscreenDisablers.
@interface ScopedFullscreenDisablerWrapper : NSObject {
  // The disabler that prevents the toolbar from being hidden while the
  // coordinator's UI is started.
  std::unique_ptr<ScopedFullscreenDisabler> _disabler;
}

// The FullscreenController being disabled.
@property(nonatomic, readonly) FullscreenController* controller;

// Factory method that returns the disabler wrapper associated with
// `coordinator`, lazily instantiating it if necessary.
+ (instancetype)wrapperForCoordinator:(ChromeCoordinator*)coordinator;

// Initializer for a wrapper that disables `controller`.
- (instancetype)initWithFullscreenController:(FullscreenController*)controller
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Creates and resets the diabler.
- (void)createDisabler;
- (void)resetDisabler;

@end

@implementation ScopedFullscreenDisablerWrapper
@synthesize controller = _controller;

+ (instancetype)wrapperForCoordinator:(ChromeCoordinator*)coordinator {
  // ChromeCoordinators that need to disable fullscreen must be initialized with
  // a ProfileIOS.
  ProfileIOS* profile = coordinator.browser->GetProfile();
  DCHECK(profile);
  // Fetch the associated wrapper.
  ScopedFullscreenDisablerWrapper* wrapper =
      objc_getAssociatedObject(coordinator, kFullscreenDisablerKey);
  if (!wrapper) {
    FullscreenController* controller =
        FullscreenController::FromBrowser(coordinator.browser);
    wrapper = [[ScopedFullscreenDisablerWrapper alloc]
        initWithFullscreenController:controller];
    objc_setAssociatedObject(coordinator, kFullscreenDisablerKey, wrapper,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  DCHECK(wrapper);
  return wrapper;
}

- (instancetype)initWithFullscreenController:(FullscreenController*)controller {
  if ((self = [super init])) {
    _controller = controller;
    DCHECK(_controller);
  }
  return self;
}

- (void)createDisabler {
  _disabler = std::make_unique<ScopedFullscreenDisabler>(self.controller);
}

- (void)resetDisabler {
  _disabler = nullptr;
}

@end

#pragma mark - ChromeCoordinator

@implementation ChromeCoordinator (FullscreenDisabling)

- (void)didStartFullscreenDisablingUI {
  [[ScopedFullscreenDisablerWrapper wrapperForCoordinator:self] createDisabler];
}

- (void)didStopFullscreenDisablingUI {
  [[ScopedFullscreenDisablerWrapper wrapperForCoordinator:self] resetDisabler];
}

@end
