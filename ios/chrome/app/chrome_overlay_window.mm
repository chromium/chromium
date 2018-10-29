// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_overlay_window.h"

#include "base/logging.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/metrics/drag_and_drop_recorder.h"
#import "ios/chrome/browser/metrics/size_class_recorder.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeOverlayWindow () {
  SizeClassRecorder* _sizeClassRecorder;
  DragAndDropRecorder* _dragAndDropRecorder;
}

// Initializes the size class recorder. On iPad It starts tracking horizontal
// size class changes.
- (void)initializeSizeClassRecorder;

// Updates the Breakpad report with the current size class.
- (void)updateBreakpad;

@end

@implementation ChromeOverlayWindow

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // When not created via a nib, create the recorders immediately.
    [self initializeSizeClassRecorder];
    [self updateBreakpad];
    _dragAndDropRecorder = [[DragAndDropRecorder alloc] initWithView:self];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  // When creating via a nib, wait to be awoken, as the size class is not
  // reliable before.
  [self initializeSizeClassRecorder];
  [self updateBreakpad];
}

- (void)initializeSizeClassRecorder {
  DCHECK(!_sizeClassRecorder);
  if (IsIPadIdiom()) {
    _sizeClassRecorder = [[SizeClassRecorder alloc]
        initWithHorizontalSizeClass:self.traitCollection.horizontalSizeClass];
  }
}

- (void)updateBreakpad {
  breakpad_helper::SetCurrentHorizontalSizeClass(
      self.traitCollection.horizontalSizeClass);
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    [_sizeClassRecorder
        horizontalSizeClassDidChange:self.traitCollection.horizontalSizeClass];
    [self updateBreakpad];
  }
}

#pragma mark - Testing methods

- (void)unsetSizeClassRecorder {
  _sizeClassRecorder = nil;
}

@end
