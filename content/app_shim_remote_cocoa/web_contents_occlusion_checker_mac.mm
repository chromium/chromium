// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"

#include "base/auto_reset.h"
#include "base/feature_list.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"

namespace {

const base::mac::ScopedObjCClassSwizzler* GetWindowClassSwizzler() {
  static const base::NoDestructor<base::mac::ScopedObjCClassSwizzler>
      window_class_swizzler([NSWindow class],
                            [WebContentsOcclusionCheckerMac class],
                            @selector(orderWindow:relativeTo:));
  return window_class_swizzler.get();
}

const base::Feature kMacWebContentsOcclusion{"MacWebContentsOcclusion",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool> kEnhancedWindowOcclusionDetection{
    &kMacWebContentsOcclusion, "EnhancedWindowOcclusionDetection", false};
const base::FeatureParam<bool> kDisplaySleepAndAppHideDetection{
    &kMacWebContentsOcclusion, "DisplaySleepAndAppHideDetection", false};

}  // namespace

@interface WebContentsOcclusionCheckerMac () {
  NSWindow* _windowResizingMovingOrClosing;
  NSWindow* _windowReceivingFullscreenTransitionNotifications;
  BOOL _displaysAreAsleep;
}
// Computes and returns the `window`'s visibility state, a hybrid of
// macOS's and our manual occlusion calculation.
- (remote_cocoa::mojom::Visibility)contentVisibilityStateForWindow:
    (NSWindow*)window;
- (void)updateWebContentsVisibilityInWindow:(NSWindow*)window;

@end

@implementation WebContentsOcclusionCheckerMac

+ (instancetype)sharedInstance {
  static WebContentsOcclusionCheckerMac* sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[self alloc] init];
    if (kEnhancedWindowOcclusionDetection.Get()) {
      GetWindowClassSwizzler();
    }
  });
  return sharedInstance;
}

- (instancetype)init {
  self = [super init];

  [self setUpNotifications];

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];

  [super dealloc];
}

// Alternative implementation of orderWindow:relativeTo:. Replaces
// NSWindow's version, allowing the occlusion checker to learn about
// window ordering events.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  // Super.
  GetWindowClassSwizzler()
      ->InvokeOriginal<void, NSWindowOrderingMode, NSInteger>(
          self, _cmd, orderingMode, otherWindowNumber);

  // The window order has changed so update web contents visibility.
  [[WebContentsOcclusionCheckerMac sharedInstance]
      notifyUpdateWebContentsVisibility];
}

- (void)setUpNotifications {
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if (kEnhancedWindowOcclusionDetection.Get()) {
    [notificationCenter addObserver:self
                           selector:@selector(windowWillMove:)
                               name:NSWindowWillMoveNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(windowDidMove:)
                               name:NSWindowDidMoveNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(windowWillStartLiveResize:)
                               name:NSWindowWillStartLiveResizeNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(windowWillEndLiveResize)
                               name:NSWindowDidEndLiveResizeNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification
                             object:nil];
  }
  [notificationCenter addObserver:self
                         selector:@selector(windowChangedOcclusionState:)
                             name:NSWindowDidChangeOcclusionStateNotification
                           object:nil];

  [notificationCenter addObserver:self
                         selector:@selector(fullscreenTransitionStarted:)
                             name:NSWindowWillEnterFullScreenNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(fullscreenTransitionComplete:)
                             name:NSWindowDidEnterFullScreenNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(fullscreenTransitionStarted:)
                             name:NSWindowWillExitFullScreenNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(fullscreenTransitionComplete:)
                             name:NSWindowDidExitFullScreenNotification
                           object:nil];

  if (kDisplaySleepAndAppHideDetection.Get()) {
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(displaysDidSleep:)
               name:NSWorkspaceScreensDidSleepNotification
             object:nil];
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(displaysDidWake:)
               name:NSWorkspaceScreensDidWakeNotification
             object:nil];
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  base::AutoReset<NSWindow*> tmp(&_windowResizingMovingOrClosing,
                                 [notification object]);
  [self notifyUpdateWebContentsVisibility];
}

- (void)windowWillMove:(NSNotification*)notification {
  base::AutoReset<NSWindow*> tmp(&_windowResizingMovingOrClosing,
                                 [notification object]);
  [self notifyUpdateWebContentsVisibility];
}

- (void)windowDidMove:(NSNotification*)notification {
  [self notifyUpdateWebContentsVisibility];
}

- (void)windowWillStartLiveResize:(NSNotification*)notification {
  _windowResizingMovingOrClosing = [notification object];
  [self notifyUpdateWebContentsVisibility];
}

- (void)windowWillEndLiveResize {
  _windowResizingMovingOrClosing = nil;
  [self notifyUpdateWebContentsVisibility];
}

- (void)windowChangedOcclusionState:(NSNotification*)notification {
  [self updateWebContentsVisibilityInWindow:[notification object]];
}

- (void)displaysDidSleep:(NSNotification*)notification {
  _displaysAreAsleep = YES;
  [self notifyUpdateWebContentsVisibility];
}

- (void)displaysDidWake:(NSNotification*)notification {
  _displaysAreAsleep = NO;
  [self notifyUpdateWebContentsVisibility];
}

- (void)fullscreenTransitionStarted:(NSNotification*)notification {
  _windowReceivingFullscreenTransitionNotifications = [notification object];
}

- (void)fullscreenTransitionComplete:(NSNotification*)notification {
  _windowReceivingFullscreenTransitionNotifications = nil;
}

- (void)notifyUpdateWebContentsVisibility {
  // https://crbug.com/1300929 covers a crash where a webcontents gets added to
  // a window, triggering an update to its visibility state. A visibility state
  // observer creates a bubble, and that bubble triggers a call to
  // -notifyUpdateWebContentsVisibility. -notifyUpdateWebContentsVisibility goes
  // on to update the occlusion status of all all windows, which triggers the
  // visibility state observer a second time, leading to another bubble
  // creation, another call to -notifyUpdateWebContentsVisibility, and then a
  // crash. We could prevent -notifyUpdateWebContentsVisibility from being
  // reentered but that could still result in a visibility state observer
  // entering its observer code twice (as happened in the bug). By making the
  // occlusion status update occur away from the notification we can avoid the
  // reentrancy problems with visibility observers.
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (_notifyUpdateWebContentsVisibility)
                                             object:nil];
  [self performSelector:@selector(_notifyUpdateWebContentsVisibility)
             withObject:nil
             afterDelay:0];
}

- (void)_notifyUpdateWebContentsVisibility {
  for (NSWindow* window in [[NSApplication sharedApplication] orderedWindows]) {
    [self updateWebContentsVisibilityInWindow:window];
  }
}

- (void)updateWebContentsVisibilityInWindow:(NSWindow*)window {
  // The fullscreen transition causes spurious occlusion notifications.
  // See https://crbug.com/1081229
  if (window == _windowReceivingFullscreenTransitionNotifications)
    return;

  // If there's no web contents in the window there's nothing to do.
  NSArray<WebContentsViewCocoa*>* webContentsViewCocoaInWindow =
      [window webContentsViewCocoa];
  if (webContentsViewCocoaInWindow.count == 0) {
    return;
  }

  remote_cocoa::mojom::Visibility windowVisibilityState =
      [self contentVisibilityStateForWindow:window];

  for (WebContentsViewCocoa* webContentsViewCocoa in
           webContentsViewCocoaInWindow) {
    remote_cocoa::mojom::Visibility visibilityState =
        [webContentsViewCocoa isHiddenOrHasHiddenAncestor]
            ? remote_cocoa::mojom::Visibility::kHidden
            : windowVisibilityState;
    [webContentsViewCocoa updateWebContentsVisibility:visibilityState];
  }
}

- (remote_cocoa::mojom::Visibility)contentVisibilityStateForWindow:
    (NSWindow*)window {
  if (_displaysAreAsleep) {
    return remote_cocoa::mojom::Visibility::kHidden;
  }

  BOOL windowOccluded =
      !([window occlusionState] & NSWindowOcclusionStateVisible);
  if (windowOccluded) {
    // If macOS says the window is occluded, take that answer.
    return remote_cocoa::mojom::Visibility::kOccluded;
  }

  // If manual occlusion detection is disabled in the experiement, return the
  // answer from macOS.
  if (!kEnhancedWindowOcclusionDetection.Get()) {
    return remote_cocoa::mojom::Visibility::kVisible;
  }

  NSRect windowFrame = [window frame];

  NSArray<NSWindow*>* windowsFromFrontToBack =
      [[NSApplication sharedApplication] orderedWindows];

  // Determine if there's a window occluding our window.
  for (NSWindow* nextWindow in windowsFromFrontToBack) {
    if (![nextWindow isVisible]) {
      continue;
    }

    // If we come to our window in the list we're done.
    if (nextWindow == window) {
      break;
    }

    // If the next window is closing, moving, or resizing, treat it as if it
    // doesn't exist so that if it currently occludes our web contents we
    // transition from kOccluded to kVisible. That way our content, if it
    // becomes visible, is fresh. We'll recompute our visibility status after
    // the resize, move, or close completes.
    if (nextWindow == _windowResizingMovingOrClosing) {
      continue;
    }

    // Ideally we'd compute the region which is the sum of all windows above us
    // and see if it completely contains our web contents. Unfortunately we
    // don't have a library that can perform general purpose region arithmetic.
    // For example if we have windows A and B side-by-side at first glance it
    // might seem like enough to just union the two frames. The problem is the
    // small regions at the top and bottom of the windows where the curved
    // corners meet. If A and B cover C we can't know if a portion of C shows
    // through these regions. The best we can do is see if any single browser
    // window above completely contains our frame.
    if (NSContainsRect([nextWindow frame], windowFrame)) {
      return remote_cocoa::mojom::Visibility::kOccluded;
    }
  }

  return remote_cocoa::mojom::Visibility::kVisible;
}

- (void)updateWebContentsVisibility:
    (WebContentsViewCocoa*)webContentsViewCocoa {
  remote_cocoa::mojom::Visibility contentVisibilityState =
      [self contentVisibilityStateForWindow:[webContentsViewCocoa window]];

  [webContentsViewCocoa updateWebContentsVisibility:contentVisibilityState];
}

@end
