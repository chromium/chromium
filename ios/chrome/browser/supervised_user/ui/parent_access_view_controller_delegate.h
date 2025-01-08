// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_VIEW_CONTROLLER_DELEGATE_H_

#import "base/functional/callback.h"

using AuthenticatedURLCallback = base::OnceCallback<void(NSURL*, NSError*)>;

// Delegate for operations on the parent access widget.
@protocol ParentAccessViewControllerDelegate

// Handles the request to load the parent access widget.
- (void)handleParentAccessRequest:(AuthenticatedURLCallback)callback;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_VIEW_CONTROLLER_DELEGATE_H_
