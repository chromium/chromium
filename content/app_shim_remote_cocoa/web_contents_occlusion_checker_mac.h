// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_

#import <AppKit/AppKit.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"

extern CONTENT_EXPORT const base::FeatureParam<bool>
    kEnhancedWindowOcclusionDetection;
extern CONTENT_EXPORT const base::FeatureParam<bool>
    kDisplaySleepAndAppHideDetection;

// The WebContentsOcclusionCheckerMac performs window occlusion checks
// for browser windows of similar size, a case macOS's occlusion
// detection system cannot handle (see crbug.com/883031). It initiates
// these checks on window state change events (ordering, closing, etc.).
//
// The checker makes it easy to learn of window occlusion events from
// both macOS and from its own occlusion checks. Interested parties should
// watch for NSWindowDidChangeOcclusionStateNotification notifications
// where the object is the affected window and the userInfo contains the
// key "WebContentsOcclusionCheckerMac".
@interface WebContentsOcclusionCheckerMac : NSObject

+ (instancetype)sharedInstance;

// Returns YES if the specified version is less than 13.0 or more than 13.2.
// Manual occlusion detection is not supported on macOS 13.0-13.2.
+ (BOOL)manualOcclusionDetectionSupportedForPackedVersion:(int)version;

// Returns YES if manual occlusion detection is supported for the current macOS.
+ (BOOL)manualOcclusionDetectionSupportedForCurrentMacOSVersion;

// API exposed for testing.

// Resets the state of `sharedInstance` during tests.
+ (void)resetSharedInstanceForTesting;

// Schedules an occlusion state update for all windows with web contentses.
- (void)scheduleOcclusionStateUpdates;

// Updates the occlusion states of all windows with web contentses.
- (void)performOcclusionStateUpdates;

// Returns YES if occlusion updates are scheduled.
- (BOOL)occlusionStateUpdatesAreScheduledForTesting;

@end

@interface NSWindow (WebContentsOcclusionCheckerMac)
@property(nonatomic, assign, getter=isOccluded) BOOL occluded;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_
