// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"

#import "base/check.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/metrics/model/user_interface_style_recorder.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_container_view.h"
#import "ui/base/device_form_factor.h"

@interface ChromeOverlayWindow ()
// A container view for all the overlays.
@property(nonatomic, strong, readonly)
    ChromeOverlayContainerView* overlayContainerView;
@end

@implementation ChromeOverlayWindow {
  // Sorted list of active overlays.
  NSMutableArray<UIView*>* _overlays;
  // Map of overlays to their window level.
  NSMapTable<UIView*, NSNumber*>* _overlayLevels;
  // Recorder for UI style metrics.
  UserInterfaceStyleRecorder* _userInterfaceStyleRecorder;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUp];
  }
  return self;
}

- (void)activateOverlay:(UIView*)overlay withLevel:(UIWindowLevel)level {
  CHECK(overlay);
  CHECK(![_overlays containsObject:overlay]);

  // Find the correct insertion index to keep the array sorted by level.
  NSUInteger insertionIndex = _overlays.count;
  for (NSUInteger i = 0; i < _overlays.count; ++i) {
    UIView* existingOverlay = _overlays[i];
    NSNumber* existingLevelNumber =
        [_overlayLevels objectForKey:existingOverlay];
    if (level < [existingLevelNumber doubleValue]) {
      insertionIndex = i;
      break;
    }
  }

  // Size the overlay to fit the container.
  overlay.frame = _overlayContainerView.bounds;
  overlay.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  // Add the overlay to the container and our tracking arrays.
  [_overlayLevels setObject:@(level) forKey:overlay];
  [_overlays insertObject:overlay atIndex:insertionIndex];
  [_overlayContainerView insertSubview:overlay atIndex:insertionIndex];

  [self updateOverlayContainerHiddenState];
}

- (void)deactivateOverlay:(UIView*)overlay {
  CHECK(overlay);
  CHECK([_overlays containsObject:overlay]);
  [_overlayLevels removeObjectForKey:overlay];
  [_overlays removeObject:overlay];
  [overlay removeFromSuperview];

  [self updateOverlayContainerHiddenState];
}

- (void)didAddSubview:(UIView*)subview {
  [super didAddSubview:subview];
  // Ensure that the overlay container is always on top of other views.
  if (subview != _overlayContainerView) {
    [self bringSubviewToFront:_overlayContainerView];
  }
}

- (void)sendSubviewToBack:(UIView*)view {
  // Don't allow the overlay container to be sent to the back.
  if (view == _overlayContainerView) {
    return;
  }
  [super sendSubviewToBack:view];
}

#pragma mark - Private

// Initial setup.
- (void)setUp {
  _overlays = [NSMutableArray array];
  _overlayLevels = [NSMapTable weakToStrongObjectsMapTable];
  self.backgroundColor = [UIColor clearColor];

  _overlayContainerView =
      [[ChromeOverlayContainerView alloc] initWithFrame:self.bounds];
  _overlayContainerView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _overlayContainerView.hidden = YES;
  [self addSubview:_overlayContainerView];

  // When not created via a nib, create the recorders immediately.
  [self updateCrashKeys];
  _userInterfaceStyleRecorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:self.traitCollection.userInterfaceStyle];
  NSArray<UITrait>* traits =
      @[ UITraitHorizontalSizeClass.class, UITraitUserInterfaceStyle.class ];
  [self registerForTraitChanges:traits withAction:@selector(updateCrashKeys)];
}

// Shows or hides the overlay container view based on whether there are any
// active overlays.
- (void)updateOverlayContainerHiddenState {
  _overlayContainerView.hidden = (_overlays.count == 0);
}

// Updates the crash keys with the current size class.
- (void)updateCrashKeys {
  crash_keys::SetCurrentHorizontalSizeClass(
      self.traitCollection.horizontalSizeClass);
  crash_keys::SetCurrentUserInterfaceStyle(
      self.traitCollection.userInterfaceStyle);
}

@end
