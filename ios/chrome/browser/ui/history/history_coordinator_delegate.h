// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_DELEGATE_H_

#import "base/ios/block_types.h"

// Delegate for HistoryCoordinator.
@protocol HistoryCoordinatorDelegate

// Called when the history should be dismissed.
// `Completion` is called after the dismissal but before the coordinator
// is stopped.
- (void)closeHistoryWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_DELEGATE_H_
