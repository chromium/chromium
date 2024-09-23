// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_
#define IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"

// Logs metrics about the storage used by the application. `profile_path`
// must point to the main user non-incognito profile directory and
// `off_the_record_state_path` to the incognito state path.
void LogApplicationStorageMetrics(base::FilePath profile_path,
                                  base::FilePath off_the_record_state_path);

#endif  // IOS_CHROME_APP_APPLICATION_STORAGE_METRICS_H_
