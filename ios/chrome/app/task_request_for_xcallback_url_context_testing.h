// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_TESTING_H_
#define IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_TESTING_H_

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

class TemplateURLService;

// Methods exposed for testing, as Lens isn't available in unit tests.
@interface TaskRequestForXCallbackURLContext (Testing)

// Returns whether images must be searched with Lens.
- (BOOL)shouldSearchImageWithLens:(TemplateURLService*)templateURLService;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_FOR_XCALLBACK_URL_CONTEXT_TESTING_H_
