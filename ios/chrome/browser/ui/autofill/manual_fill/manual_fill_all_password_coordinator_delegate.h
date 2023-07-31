// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_

@class ManualFillAllPasswordCoordinator;

@protocol ManualFillAllPasswordCoordinatorDelegate <NSObject>

- (void)manualFillAllPasswordCoordinatorWantsToBeDismissed:
    (ManualFillAllPasswordCoordinator*)coordinator;
@end
#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_
