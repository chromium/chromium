// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_TEST_SCREEN_CAPTURE_KIT_TEST_HELPER_H_
#define MEDIA_CAPTURE_VIDEO_MAC_TEST_SCREEN_CAPTURE_KIT_TEST_HELPER_H_

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/apple/foundation_util.h"

@interface FakeSCRunningApplication : NSObject
@property(readonly) pid_t processID;
@property(readonly, copy) NSString* applicationName;
@property(readonly, copy) NSString* bundleIdentifier;
- (instancetype)initWithProcessID:(pid_t)pid
                  applicationName:(NSString*)name
                 bundleIdentifier:(NSString*)bundleIdentifier;
@end

@interface FakeSCWindow : NSObject
@property(readonly) CGWindowID windowID;
@property(readonly) CGRect frame;
@property(readonly, copy) NSString* title;
@property(readonly) NSInteger windowLayer;
@property(readonly) FakeSCRunningApplication* owningApplication;
@property(readonly) BOOL onScreen;
- (instancetype)initWithID:(CGWindowID)wid
                     title:(NSString*)title
         owningApplication:(FakeSCRunningApplication*)app
               windowLayer:(NSInteger)layer
                     frame:(CGRect)frame
                  onScreen:(BOOL)onScreen;
@end

@interface FakeSCDisplay : NSObject
@property(readonly) CGDirectDisplayID displayID;
@property(readonly) CGRect frame;
@property(readonly) NSInteger width;
@property(readonly) NSInteger height;
- (instancetype)initWithID:(CGDirectDisplayID)did frame:(CGRect)frame;
@end

@interface FakeSCShareableContent : NSObject
@property(readonly, copy) NSArray<FakeSCWindow*>* windows;
@property(readonly, copy) NSArray<FakeSCDisplay*>* displays;
- (instancetype)initWithWindows:(NSArray<FakeSCWindow*>*)windows
                       displays:(NSArray<FakeSCDisplay*>*)displays;
@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_TEST_SCREEN_CAPTURE_KIT_TEST_HELPER_H_
