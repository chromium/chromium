// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_WEB_STATE_DEFERRED_EXECUTOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_WEB_STATE_DEFERRED_EXECUTOR_H_

#import "base/ios/block_types.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

// Defines a block to be executed once the web state is loaded, providing a
// boolean indicating success.
typedef void (^WebStateLoadedCompletionBlock)(BOOL success);

// Utilitary to delay execution until the web state is loaded.
@interface WebStateDeferredExecutor : NSObject <CRWWebStateObserver>

// Executes the given `completion` once the web state is loaded.
- (void)webState:(web::WebState*)webState
    executeOnceLoaded:(WebStateLoadedCompletionBlock)completion;

// Executes the given `completion` once the web state is realized.
- (void)webState:(web::WebState*)webState
    executeOnceRealized:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_WEB_STATE_DEFERRED_EXECUTOR_H_
