// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_DELEGATE_H_

#import "base/ios/block_types.h"

// Handles Tips module events.
@protocol TipsMagicStackMediatorDelegate

// Signals that the TipsMagicStackMediator did reconfigure the existing item.
- (void)tipsMagicStackMediatorDidReconfigureItem;

// Indicates to receiver that the Tips module should be removed.
// The `completion` is called after the removal is finished.
- (void)removeTipsModuleWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_DELEGATE_H_
