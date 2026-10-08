// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_READ_IMAGE_TRACE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_READ_IMAGE_TRACE_H_

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// Helper class that encapsulates a Perfetto flow ID and event names for
// tracing snapshot image loading.
@interface SnapshotReadImageTrace : NSObject

// Initializes a trace instance with a newly generated globally unique flow ID
// and records an instant "post" trace event.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Wraps `block` inside the background task perfetto slice.
- (void)backgroundTask:(void(NS_NOESCAPE ^)(void))block;

// Wraps `block` inside the completion handler perfetto slice.
- (void)completion:(void(NS_NOESCAPE ^)(void))block;

// Wraps `block` inside the decode slice and returns the result.
- (UIImage*)decode:(UIImage*(NS_NOESCAPE ^)(void))block;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_READ_IMAGE_TRACE_H_
