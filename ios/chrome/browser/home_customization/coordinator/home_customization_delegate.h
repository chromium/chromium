// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DELEGATE_H_

// Delegate protocol for the HomeCustomizationCoordinator to communicate with
// the NewTabPageCoordinator.
@protocol HomeCustomizationDelegate

// Called when the presented customization menu is dismissed.
- (void)handleCustomizationMenuDismissed:
    (HomeCustomizationCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DELEGATE_H_
