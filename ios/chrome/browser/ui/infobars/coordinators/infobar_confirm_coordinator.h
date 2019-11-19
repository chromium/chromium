// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_CONFIRM_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_CONFIRM_COORDINATOR_H_

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

class ConfirmInfoBarDelegate;

// Coordinator that creates and manages the ConfirmInfobar.
@interface InfobarConfirmCoordinator : InfobarCoordinator

- (instancetype)initWithInfoBarDelegate:
                    (ConfirmInfoBarDelegate*)confirmInfoBarDelegate
                           badgeSupport:(BOOL)badgeSupport
                                   type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_CONFIRM_COORDINATOR_H_
