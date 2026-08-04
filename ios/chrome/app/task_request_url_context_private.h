// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
#define IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/task_request_url_context.h"

// Class extension declaring private properties and methods of
// TaskRequestForURLContext for use by its subclasses.
@interface TaskRequestForURLContext ()

@property(nonatomic, strong, readonly) UIOpenURLContext* URLContext;

- (void)recordStartupMetrics;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
