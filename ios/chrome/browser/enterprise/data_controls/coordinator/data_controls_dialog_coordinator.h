// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COORDINATOR_DATA_CONTROLS_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COORDINATOR_DATA_CONTROLS_DIALOG_COORDINATOR_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the Data Controls dialog.
//
// It is responsible for showing a warning dialog to the user when a
// sensitive data action is taken. The dialog warns the user and allows them to
// either proceed or cancel the action.
@interface DataControlsDialogCoordinator : ChromeCoordinator

// Initializes a new instance of `DataControlsDialogCoordinator`.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    dialogType:
                        (data_controls::DataControlsDialog::Type)dialogType
            organizationDomain:(std::string_view)organizationDomain
                      callback:(base::OnceCallback<void(bool)>)callback;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COORDINATOR_DATA_CONTROLS_DIALOG_COORDINATOR_H_
