// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/test/screen_capture_kit_test_helper.h"

@implementation FakeSCRunningApplication
@synthesize processID = _processID;
@synthesize applicationName = _applicationName;
@synthesize bundleIdentifier = _bundleIdentifier;

- (instancetype)initWithProcessID:(pid_t)pid
                  applicationName:(NSString*)name
                 bundleIdentifier:(NSString*)bundleIdentifier {
  if (self = [super init]) {
    _processID = pid;
    _applicationName = [name copy];
    _bundleIdentifier = [bundleIdentifier copy];
  }
  return self;
}
@end

@implementation FakeSCWindow
@synthesize windowID = _windowID;
@synthesize frame = _frame;
@synthesize title = _title;
@synthesize windowLayer = _windowLayer;
@synthesize owningApplication = _owningApplication;
@synthesize onScreen = _onScreen;

- (instancetype)initWithID:(CGWindowID)wid
                     title:(NSString*)title
         owningApplication:(FakeSCRunningApplication*)app
               windowLayer:(NSInteger)layer
                     frame:(CGRect)frame
                  onScreen:(BOOL)onScreen {
  if (self = [super init]) {
    _windowID = wid;
    _title = [title copy];
    _owningApplication = app;
    _windowLayer = layer;
    _frame = frame;
    _onScreen = onScreen;
  }
  return self;
}
@end

@implementation FakeSCDisplay
@synthesize displayID = _displayID;
@synthesize frame = _frame;

- (instancetype)initWithID:(CGDirectDisplayID)did frame:(CGRect)frame {
  if (self = [super init]) {
    _displayID = did;
    _frame = frame;
  }
  return self;
}

- (NSInteger)width {
  return static_cast<NSInteger>(_frame.size.width);
}

- (NSInteger)height {
  return static_cast<NSInteger>(_frame.size.height);
}
@end

@implementation FakeSCShareableContent
@synthesize windows = _windows;
@synthesize displays = _displays;

- (instancetype)initWithWindows:(NSArray<FakeSCWindow*>*)windows
                       displays:(NSArray<FakeSCDisplay*>*)displays {
  if (self = [super init]) {
    _windows = [windows copy];
    _displays = [displays copy];
  }
  return self;
}
@end
