// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_FOR_EXTERNAL_ACTION_URL_CONTEXT_H_
#define IOS_CHROME_APP_TASK_REQUEST_FOR_EXTERNAL_ACTION_URL_CONTEXT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/task_request_url_context.h"

// Host of the URLs handled by `TaskRequestForExternalActionURLContext`.
extern NSString* const kExternalActionURLHost;

// Subclass of TaskRequestForURLContext for handling external action URLs
// (e.g. googlechrome://ChromeExternalAction/OpenNTP).
@interface TaskRequestForExternalActionURLContext : TaskRequestForURLContext
@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_FOR_EXTERNAL_ACTION_URL_CONTEXT_H_
