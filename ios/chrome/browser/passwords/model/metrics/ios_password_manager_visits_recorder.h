// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_VISITS_RECORDER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_VISITS_RECORDER_H_

#import <Foundation/Foundation.h>

namespace password_manager {
enum class PasswordManagerSurface;
}

// Helper class for logging visits to Password Manager surfaces on iOS.
@interface IOSPasswordManagerVisitsRecorder : NSObject

// Creates an instance for logging visits to a given Password Manager surface.
- (instancetype)initWithPasswordManagerSurface:
    (password_manager::PasswordManagerSurface)surface;

// Records one visit if one hasn't been recorded before.
- (void)maybeRecordVisitMetric;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_VISITS_RECORDER_H_
