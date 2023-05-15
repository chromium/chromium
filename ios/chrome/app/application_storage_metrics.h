// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_
#define IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"

// A key to NSUserDefaults storing the last time these metrics were logged.
extern NSString* const kLastApplicationStorageMetricsLogTime;

// Logs metrics about the storage used by the application and then updates the
// `kLastApplicationStorageMetricsLogTime` user default value. `profile_path`
// must point to the main user profile directory in order to log metrics about
// Optimization Guide storage usage.
void LogApplicationStorageMetrics(base::FilePath profile_path);

#endif  // IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_
