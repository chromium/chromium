// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_FOR_REMOTE_ACTOR_URL_CONTEXT_H_
#define IOS_CHROME_APP_TASK_REQUEST_FOR_REMOTE_ACTOR_URL_CONTEXT_H_

#import "ios/chrome/app/task_request_url_context.h"

// Subclass of TaskRequestForURLContext for handling Remote Actor credential
// sharing URLs.
@interface TaskRequestForRemoteActorURLContext : TaskRequestForURLContext
@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_FOR_REMOTE_ACTOR_URL_CONTEXT_H_
