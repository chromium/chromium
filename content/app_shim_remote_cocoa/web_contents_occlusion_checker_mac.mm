// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"

#include <memory>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

using features::kMacWebContentsOcclusion;

// Experiment features.
const base::FeatureParam<bool> kEnhancedWindowOcclusionDetection{
    &kMacWebContentsOcclusion, "EnhancedWindowOcclusionDetection", false};

namespace {

NSString* const kWindowDidChangePositionInWindowList =
    @"ChromeWindowDidChangePositionInWindowList";
constexpr char kWindowIsOccludedKey[] = "ChromeWindowIsOccludedKey";

bool IsBrowserProcess() {
  return base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueASCII("type")
      .empty();
}

}  // namespace

@interface WebContentsOcclusionCheckerMac ()

// Returns a pointer to the shared instance that can be cleared during tests.
+ (WebContentsOcclusionCheckerMac* __strong*)sharedOcclusionChecker;

- (base::apple::ScopedObjCClassSwizzler*)windowClassSwizzler;

@end

@implementation WebContentsOcclusionCheckerMac {
  NSWindow* __weak _windowResizingOrMoving;
  NSWindow* __weak _windowReceivingFullscreenTransitionNotifications;
  BOOL _displaysAreAsleep;
  BOOL _occlusionStateUpdatesAreScheduled;
  BOOL _updatingOcclusionStates;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> _windowClassSwizzler;
}

+ (WebContentsOcclusionCheckerMac* __strong*)sharedOcclusionChecker {
  static WebContentsOcclusionCheckerMac* sharedOcclusionChecker;
  return &sharedOcclusionChecker;
}

+ (instancetype)sharedInstance {
  WebContentsOcclusionCheckerMac* __strong* sharedInstance =
      [self sharedOcclusionChecker];

  // It seems a utility process can trigger a call to
  // +[WebContentsViewCocoa initialize] (from a non-main thread!), which calls
  // out to here to create the occlusion tracker. To guard against that, and
  // any other other potential callers, only create the tracker if we're
  // running inside the browser process. https://crbug.com/349984532 .
  if (*sharedInstance == nil && IsBrowserProcess()) {
    *sharedInstance = [[self alloc] init];
  }

  return *sharedInstance;
}

+ (BOOL)manualOcclusionDetectionSupportedForPackedVersion:(int)version {
  if (version >= 13'00'00 && version < 13'03'00) {
    return NO;
  }

  return YES;
}

+ (BOOL)manualOcclusionDetectionSupportedForCurrentMacOSVersion {
  return [self manualOcclusionDetectionSupportedForPackedVersion:
                   base::mac::MacOSVersion()];
}

+ (void)resetSharedInstanceForTesting {
  *[self sharedOcclusionChecker] = nil;
}

- (instancetype)init {
  self = [super init];

  DCHECK(base::FeatureList::IsEnabled(kMacWebContentsOcclusion));
  DCHECK(IsBrowserProcess());
  if (!IsBrowserProcess()) {
    static auto* const crash_key = base::debug::AllocateCrashKeyString(
        "MacWebContentsOcclusionChecker", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(crash_key, "initialized");
  }

  [self setUpNotifications];

  // There's no notification for NSWindows changing their order in the window
  // list. Swizzle -orderWindow:relativeTo:, allowing the checker to initiate
  // occlusion checks on window ordering changes.
  _windowClassSwizzler = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
      [NSWindow class], [WebContentsOcclusionCheckerMac class],
      @selector(orderWindow:relativeTo:));

  return self;
}

- (void)dealloc {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (performOcclusionStateUpdates)
                                             object:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  _windowClassSwizzler.reset();
}

- (base::apple::ScopedObjCClassSwizzler*)windowClassSwizzler {
  return _windowClassSwizzler.get();
}

- (BOOL)isManualOcclusionDetectionEnabled {
  return [WebContentsOcclusionCheckerMac
             manualOcclusionDetectionSupportedForCurrentMacOSVersion] &&
         kEnhancedWindowOcclusionDetection.Get();
}

// Alternative implementation of orderWindow:relativeTo:. Replaces
// NSWindow's version, allowing the occlusion checker to learn about
// window ordering events.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  // Super.
  [[WebContentsOcclusionCheckerMac sharedInstance] windowClassSwizzler]
      ->InvokeOriginal<void, NSWindowOrderingMode, NSInteger>(
          self, _cmd, orderingMode, otherWindowNumber);

  if (![[WebContentsOcclusionCheckerMac sharedInstance]
          isManualOcclusionDetectionEnabled]) {
    return;
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kWindowDidChangePositionInWindowList
                    object:self
                  userInfo:nil];
}

- (void)setUpNotifications {
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if ([self isManualOcclusionDetectionEnabled]) {
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
                           selector:@selector(windowWillEndLiveResize:)
                               name:NSWindowDidEndLiveResizeNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification
                             object:nil];
    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangePositionInWindowList:)
               name:kWindowDidChangePositionInWindowList
             object:nil];

    // orderWindow:relativeTo: was the override point that caught all window
    // list ordering changes up until Sonoma. With Sonoma, it appears that
    // window cycling (Cmd+`) goes directly to -[NSWindow makeKeyWindow]. Add
    // these window main notifications to catch the changes. Unfortunately,
    // there doesn't appear to be a way to trigger any of the the window
    // cycling machinery, so automated testing is impossible.
    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangePositionInWindowList:)
               name:NSWindowDidBecomeMainNotification
             object:nil];

    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangePositionInWindowList:)
               name:NSWindowDidResignMainNotification
             object:nil];

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
}

- (BOOL)windowCanTriggerOcclusionUpdates:(NSWindow*)window {
  // We only care about occlusion because we want to inform web contents
  // so they can update their visibility state. Therefore, we ignore windows
  // that don't have a web contents (they essentially don't exist for our manual
  // occlusion calculations).
  if (![window containsWebContentsViewCocoa])
    return NO;

  // The checker cycles through the window list when performing its manual
  // occlusion checks. Child windows don't appear in this list, so don't
  // trigger an occlusion check unless `window` is a parent window
  // (i.e. is not the child of another window).
  if ([window parentWindow] == nil)
    return YES;

  // `window` is a child window but that wasn't always the case. When it was
  // created and resized it was a "parent" window. If it came through this
  // codepath, we may have marked it occluded. Since we don't trigger occlusion
  // updates on child windows, now that the window is a child, there's a chance
  // we'll never update its state to visible. To avoid this, ensure it's visible
  // before we exit. See https://crbug.com/1337390 .
  [window setOccluded:NO];

  return NO;
}

- (void)windowWillClose:(NSNotification*)notification {
  NSWindow* theWindow = [notification object];

  // -windowCanTriggerOcclusionUpdates: returns NO if the window doesn't
  // contain a webcontents. In some cases, however, a closing browser window
  // will be stripped of its webcontents by the time we reach this method.
  // As a result, if we use windowCanTriggerOcclusionUpdates:, the browser
  // window's closure won't trigger an occlusion state update, leaving the
  // webcontentses in windows it covered in the occluded state. To avoid this,
  // we'll perform an occlusion update if the window isn't a child window.
  // See http://crbug.com/1356622 .
  if ([theWindow parentWindow] == nil)
    [self scheduleOcclusionStateUpdates];
}

- (void)windowWillMove:(NSNotification*)notification {
  NSWindow* theWindow = [notification object];
  if (![self windowCanTriggerOcclusionUpdates:theWindow])
    return;

  _windowResizingOrMoving = theWindow;

  [self scheduleOcclusionStateUpdates];
}

- (void)windowDidMove:(NSNotification*)notification {
  // We would check _windowResizingOrMoving == nil and early out, except in
  // cases where a window is moved programmatically, windowWillMove: never gets
  // called, so _windowResizingOrMoving never gets set.
  _windowResizingOrMoving = nil;

  if ([self windowCanTriggerOcclusionUpdates:[notification object]])
    [self scheduleOcclusionStateUpdates];
}

- (void)windowDidChangePositionInWindowList:(NSNotification*)notification {
  if ([self windowCanTriggerOcclusionUpdates:[notification object]])
    [self scheduleOcclusionStateUpdates];
}

- (void)windowWillStartLiveResize:(NSNotification*)notification {
  NSWindow* theWindow = [notification object];
  if (![self windowCanTriggerOcclusionUpdates:theWindow])
    return;

  _windowResizingOrMoving = theWindow;
  [self scheduleOcclusionStateUpdates];
}

- (void)windowWillEndLiveResize:(NSNotification*)notification {
  if (_windowResizingOrMoving == nil)
    return;

  _windowResizingOrMoving = nil;

  [self scheduleOcclusionStateUpdates];
}

- (void)windowChangedOcclusionState:(NSNotification*)notification {
  // Ignore the occlusion notifications we generate.
  NSDictionary* userInfo = [notification userInfo];
  NSString* occlusionCheckerKey = [self className];
  if (userInfo[occlusionCheckerKey] != nil)
    return;

  if ([self windowCanTriggerOcclusionUpdates:[notification object]])
    [self scheduleOcclusionStateUpdates];
}

- (void)displaysDidSleep:(NSNotification*)notification {
  _displaysAreAsleep = YES;
  [self scheduleOcclusionStateUpdates];
}

- (void)displaysDidWake:(NSNotification*)notification {
  _displaysAreAsleep = NO;
  [self scheduleOcclusionStateUpdates];
}

- (void)fullscreenTransitionStarted:(NSNotification*)notification {
  // We only care about fullscreen transitions because macOS may send spurious
  // occlusion update notifications. Track the transitioning window so we can
  // ignore updates about this window until the transition is over.
  _windowReceivingFullscreenTransitionNotifications = [notification object];
}

- (void)fullscreenTransitionComplete:(NSNotification*)notification {
  _windowReceivingFullscreenTransitionNotifications = nil;
  [self scheduleOcclusionStateUpdates];
}

- (BOOL)occlusionStateUpdatesAreScheduledForTesting {
  return _occlusionStateUpdatesAreScheduled;
}

// Schedules an update of occlusion states for some time in the future.
// https://crbug.com/1300929 covers a crash where a webcontents gets added to
// a window, triggering an update to its visibility state. A visibility state
// observer creates a bubble, and that bubble triggers a call to
// -scheduleOcclusionStateUpdates. -scheduleOcclusionStateUpdates goes
// on to update the occlusion status of all windows, which triggers the
// visibility state observer a second time, leading to another bubble
// creation, another call to -scheduleOcclusionStateUpdates, and then a
// crash. We could make -scheduleOcclusionStateUpdates non-reentrant but that
// wouldn't prevent a visibility state observer from entering its observer
// code twice (as happened in the bug). By making the occlusion state
// update occur away from the notification, we can avoid the reentrancy
// problems with visibility observers.
- (void)scheduleOcclusionStateUpdates {
  if (_occlusionStateUpdatesAreScheduled)
    return;

  _occlusionStateUpdatesAreScheduled = YES;

  [self performSelector:@selector(performOcclusionStateUpdates)
             withObject:nil
             afterDelay:0];
}

- (void)performOcclusionStateUpdates {
  _occlusionStateUpdatesAreScheduled = NO;

  if (content::GetContentClient()->browser() &&
      content::GetContentClient()->browser()->IsShuttingDown()) {
    return;
  }

  DCHECK(!_updatingOcclusionStates);

  _updatingOcclusionStates = YES;

  NSArray<NSWindow*>* windowsFromFrontToBack =
      [[[NSApplication sharedApplication] orderedWindows] copy];

  for (NSWindow* window in windowsFromFrontToBack) {
    // The fullscreen transition causes spurious occlusion notifications.
    // See https://crbug.com/1081229 . Also, ignore windows that don't have
    // web contentses.
    if (window == _windowReceivingFullscreenTransitionNotifications ||
        ![window containsWebContentsViewCocoa])
      continue;

    [window setOccluded:[self isWindowOccluded:window
                                    windowList:windowsFromFrontToBack]];
  }

  _updatingOcclusionStates = NO;
}

// Returns YES if `window` is occluded, either according to macOS or via
// our manual occlusion calculation.
- (BOOL)isWindowOccluded:(NSWindow*)window
              windowList:(nonnull NSArray<NSWindow*>*)windowList {
  if (_displaysAreAsleep) {
    return YES;
  }

  BOOL windowOccludedPerMacOS =
      !([window occlusionState] & NSWindowOcclusionStateVisible);
  if (windowOccludedPerMacOS) {
    // If macOS says the window is occluded, take that answer.
    return YES;
  }

  // If manual occlusion detection is disabled, return the answer from macOS.
  if (![self isManualOcclusionDetectionEnabled]) {
    return NO;
  }

  NSRect windowFrame = [window frame];

  // Determine if there's a window occluding `window`.
  for (NSWindow* nextWindow in windowList) {
    if (![nextWindow isVisible]) {
      continue;
    }

    // If we come to our window in the list, we're done.
    if (nextWindow == window) {
      break;
    }

    // If the next window is moving or resizing, treat it as if it doesn't
    // exist so that if it currently occludes `window` it will transition to
    // visible. That way, `window`'s content, if it becomes visible, will be
    // fresh. We'll recompute `window`'s occlusion state after the move or
    // resize ends.
    if (nextWindow == _windowResizingOrMoving) {
      continue;
    }

    // Ideally we'd compute the region which is the sum of all windows above
    // `window` and see if it completely contains `window`'s web contents.
    // Unfortunately, we don't have a library that can perform general purpose
    // region arithmetic. For example, if we have windows A and B side-by-side,
    // at first glance it might seem like enough to just union the two frames.
    // The problem is the small transparent regions outside the curved window
    // corners. If A and B cover C, we can't know if a portion of C shows
    // through these regions. The best we can do is see if any single browser
    // window above completely contains our frame. This should happen more
    // frequently on a laptop, where users typically maximize their windows to
    // fit between the menu bar and dock.
    if (NSContainsRect([nextWindow frame], windowFrame)) {
      return YES;
    }
  }

  return NO;
}

@end

@implementation NSWindow (WebContentsOcclusionCheckerMac)

- (BOOL)isOccluded {
  return objc_getAssociatedObject(self, kWindowIsOccludedKey) != nil;
}

- (void)setOccluded:(BOOL)flag {
  if (flag == [self isOccluded])
    return;

  objc_setAssociatedObject(self, kWindowIsOccludedKey, flag ? @YES : nil,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);

  NSString* occlusionCheckerKey = [WebContentsOcclusionCheckerMac className];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:self
                  userInfo:@{occlusionCheckerKey : @YES}];
}

@end
