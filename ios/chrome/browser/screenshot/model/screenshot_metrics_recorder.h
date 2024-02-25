// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#ifndef IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_METRICS_RECORDER_H_

// ScreenshotMetricsRecorder presents the public interface for
// the screenshot metric collection.
@interface ScreenshotMetricsRecorder : NSObject

// Will start recording UMA metrics.
- (void)startRecordingMetrics;

@end

#endif  // IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_METRICS_RECORDER_H_
