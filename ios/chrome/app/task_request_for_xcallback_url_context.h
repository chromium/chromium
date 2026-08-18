// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_H_
#define IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_H_

#import "ios/chrome/app/task_request_url_context.h"

// Subclass of TaskRequestForURLContext for handling X-Callback URLs
// (googlechrome://x-callback-url/).
@interface TaskRequestForXCallbackURLContext : TaskRequestForURLContext

// The time when the app group command was written to shared defaults by the
// extension. Returns nil if the request is not an app group command or if the
// dispatch timestamp is missing.
@property(nonatomic, strong, readonly) NSDate* commandTime;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_H_
