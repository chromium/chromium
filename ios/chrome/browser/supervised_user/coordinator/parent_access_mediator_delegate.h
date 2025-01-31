// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for the ParentAccessMediator to update the coordinator if needed.
@protocol ParentAccessMediatorDelegate <NSObject>

// Hides the PACP bottom sheet if it fails to load before
// `kLocalWebApprovalBottomSheetLoadTimeoutMs`.
- (void)hideParentAccessBottomSheetOnTimeout;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_DELEGATE_H_
