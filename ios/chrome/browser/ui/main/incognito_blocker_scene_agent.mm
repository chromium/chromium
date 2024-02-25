// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"

@interface IncognitoBlockerSceneAgent () <SceneStateObserver>

// Scene to which this agent is attached.
// Implements the setter from SceneAgent protocol.
@property(nonatomic, weak) SceneState* sceneState;

// Interstitial view used to block any incognito tabs after backgrounding.
@property(nonatomic, strong) UIView* overlayView;

@end

@implementation IncognitoBlockerSceneAgent

#pragma mark - properties

- (UIView*)overlayView {
  if (!_overlayView) {
    // Cover the largest area potentially shown in the app switcher, in case
    // the screenshot is reused in a different orientation or size class.
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    CGFloat maxDimension =
        std::max(CGRectGetWidth(screenBounds), CGRectGetHeight(screenBounds));
    _overlayView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, maxDimension, maxDimension)];
    UIViewController* launchScreenController =
        [self loadLaunchScreenControllerFromBundle];
    [_overlayView addSubview:launchScreenController.view];
    _overlayView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
  }

  return _overlayView;
}

#pragma mark - SceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  DCHECK(!_sceneState);
  _sceneState = sceneState;
  [sceneState addObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelBackground &&
      (sceneState.incognitoContentVisible || sceneState.QRScannerVisible)) {
    // If the current BVC is incognito, or if we are in the tab switcher and
    // there are incognito tabs visible, place a full screen view containing
    // the switcher background to hide any incognito content.
    [self showOverlay];
  }

  if (level >= SceneActivationLevelForegroundInactive) {
    [self hideOverlay];
  }
}

#pragma mark - private

- (void)showOverlay {
  NSArray<UIWindow*>* windows = self.sceneState.scene.windows;

  // Adding `self.overlayView` to sceneState.window won't cover overlay windows
  // such as fullscreen video.  Instead use the topmost window.

  NSArray<UIWindow*>* sortedWindows =
      [windows sortedArrayUsingComparator:^NSComparisonResult(UIWindow* w1,
                                                              UIWindow* w2) {
        if (w1.windowLevel == w2.windowLevel) {
          return NSOrderedSame;
        }
        return w1.windowLevel < w2.windowLevel ? NSOrderedAscending
                                               : NSOrderedDescending;
      }];

  UIWindow* topWindow = sortedWindows.lastObject;
  [topWindow addSubview:self.overlayView];
}

- (void)hideOverlay {
  if (!_overlayView) {
    return;
  }
  [self.overlayView removeFromSuperview];

  // Get rid of the view to save memory.
  self.overlayView = nil;
}

- (UIViewController*)loadLaunchScreenControllerFromBundle {
  NSBundle* mainBundle = base::apple::FrameworkBundle();
  NSArray* topObjects = [mainBundle loadNibNamed:@"LaunchScreen"
                                           owner:self
                                         options:nil];
  UIViewController* launchScreenController =
      base::apple::ObjCCastStrict<UIViewController>([topObjects lastObject]);
  launchScreenController.view.autoresizingMask =
      UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
  return launchScreenController;
}

@end
