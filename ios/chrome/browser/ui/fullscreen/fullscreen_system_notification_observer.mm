// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_system_notification_observer.h"

#import <memory>

#import "base/check.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/web/common/features.h"

@interface FullscreenSystemNotificationObserver () {
  // The disabler created when VoiceOver is enabled.
  std::unique_ptr<ScopedFullscreenDisabler> _voiceOverDisabler;
  // The disabler created when the keyboard is visible.
  std::unique_ptr<ScopedFullscreenDisabler> _keyboardDisabler;
}
// The FullscreenController being enabled/disabled for system events.
@property(nonatomic, readonly, nonnull) FullscreenController* controller;
// The FullscreenMediator through which foreground events are propagated to
// FullscreenControllerObservers.
@property(nonatomic, readonly, nonnull) FullscreenMediator* mediator;
// Creates or destroys `_voiceOverDisabler` depending on whether VoiceOver is
// enabled.
- (void)voiceOverStatusChanged;
// Called when the keyboard is shown/hidden to reset `_keyboardDisabler`.
- (void)keyboardWillShow;
- (void)keyboardDidHide;
// Called when the application is foregrounded.
- (void)applicationWillEnterForeground;
@end

@implementation FullscreenSystemNotificationObserver
@synthesize controller = _controller;
@synthesize mediator = _mediator;

- (instancetype)initWithController:(FullscreenController*)controller
                          mediator:(FullscreenMediator*)mediator {
  if ((self = [super init])) {
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
    // Register for keyboard visibility notifications.
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillShow)
                          name:UIKeyboardWillShowNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardDidHide)
                          name:UIKeyboardDidHideNotification
                        object:nil];
    // Register for application lifecycle events.
    if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
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
  _controller = nullptr;
}

#pragma mark Private

- (void)voiceOverStatusChanged {
  _voiceOverDisabler =
      UIAccessibilityIsVoiceOverRunning()
          ? std::make_unique<ScopedFullscreenDisabler>(self.controller)
          : nullptr;
}

- (void)keyboardWillShow {
  if (IsNativeFindInPageAvailable()) {
    // If Native Find in Page with system Find panel is active, then triggering
    // Find in Page will show the keyboard AND make Chrome enter full screen
    // mode.
    return;
  }
  _keyboardDisabler =
      std::make_unique<ScopedFullscreenDisabler>(self.controller);
}

- (void)keyboardDidHide {
  _keyboardDisabler = nullptr;
}

- (void)applicationDidEnterBackground {
  self.mediator->ExitFullscreen();
}

- (void)applicationWillEnterForeground {
  self.mediator->ExitFullscreen();
}

@end
