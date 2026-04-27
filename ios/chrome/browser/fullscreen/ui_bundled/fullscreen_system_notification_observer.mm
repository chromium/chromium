// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_system_notification_observer.h"

#import <memory>

#import "base/check.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/legacy_fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/web/common/features.h"

@interface FullscreenSystemNotificationObserver () {
  // The disabler created when VoiceOver is enabled.
  std::unique_ptr<ScopedFullscreenDisabler> _voiceOverDisabler;
}
// The FullscreenController being enabled/disabled for system events.
@property(nonatomic, readonly, nonnull) FullscreenController* controller;
// The LegacyFullscreenMediator through which foreground events are propagated
// to FullscreenControllerObservers.
@property(nonatomic, readonly) LegacyFullscreenMediator* mediator;
// Creates or destroys `_voiceOverDisabler` depending on whether VoiceOver is
// enabled.
- (void)voiceOverStatusChanged;
// Called when the application is foregrounded.
- (void)applicationWillEnterForeground;
@end

@implementation FullscreenSystemNotificationObserver
@synthesize controller = _controller;
@synthesize mediator = _mediator;

- (instancetype)initWithController:(FullscreenController*)controller
                          mediator:(LegacyFullscreenMediator*)mediator {
  if ((self = [super init])) {
    // TODO(crbug.com/500417603): This can be removed once all calls to
    // FullscreenController are flag guarded.
    if (IsFullscreenRefactoringEnabled()) {
      return self;
    }

    _controller = controller;
    DCHECK(_controller);
    _mediator = mediator;
    DCHECK(_mediator);
    // Register for VoiceOVer status change notifications.  The notification
    // name has been updated in iOS 11.
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter
        addObserver:self
           selector:@selector(voiceOverStatusChanged)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];

    // Create a disabler if VoiceOver is enabled.
    if (UIAccessibilityIsVoiceOverRunning()) {
      _voiceOverDisabler =
          std::make_unique<ScopedFullscreenDisabler>(_controller);
    }
    // Register for application lifecycle events.
    if (ios::provider::IsFullscreenSmoothScrollingSupported()) {
      [defaultCenter addObserver:self
                        selector:@selector(applicationWillEnterForeground)
                            name:UIApplicationWillEnterForegroundNotification
                          object:nil];
    } else {
      [defaultCenter addObserver:self
                        selector:@selector(applicationDidEnterBackground)
                            name:UIApplicationDidEnterBackgroundNotification
                          object:nil];
    }
  }
  return self;
}

- (void)dealloc {
  // `-disconnect` should be called before deallocation.
  DCHECK(!_controller);
}

#pragma mark Public

- (void)disconnect {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _voiceOverDisabler.reset();
  _controller = nullptr;
  _mediator = nullptr;
}

#pragma mark Private

- (void)voiceOverStatusChanged {
  _voiceOverDisabler =
      UIAccessibilityIsVoiceOverRunning()
          ? std::make_unique<ScopedFullscreenDisabler>(self.controller)
          : nullptr;
}

- (void)applicationDidEnterBackground {
  if (!self.mediator) {
    return;
  }
  self.mediator->ExitFullscreenWithoutAnimation();
}

- (void)applicationWillEnterForeground {
  if (!self.mediator) {
    return;
  }
  self.mediator->ExitFullscreenWithoutAnimation();
}

@end
