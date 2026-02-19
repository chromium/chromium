// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_COORDINATOR_ENTERPRISE_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_COORDINATOR_ENTERPRISE_DIALOG_COORDINATOR_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the Enterprise dialog.
//
// It is responsible for showing a warning dialog to the user when a
// sensitive data action is taken. The dialog warns the user and allows them to
// either proceed or cancel the action.
@interface EnterpriseDialogCoordinator : ChromeCoordinator

// Initializes a new instance of `EnterpriseDialogCoordinator`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                dialogType:(enterprise::DialogType)dialogType
                        organizationDomain:(std::string_view)organizationDomain
                                  callback:
                                      (base::OnceCallback<void(bool)>)callback;
@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_COORDINATOR_ENTERPRISE_DIALOG_COORDINATOR_H_
