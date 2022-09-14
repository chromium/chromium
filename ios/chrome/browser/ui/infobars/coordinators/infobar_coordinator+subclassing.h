// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_SUBCLASSING_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

@interface InfobarCoordinator (Subclassing)

// Exposes a modal dismissal functionality to subclasses.
- (void)dismissInfobarModalAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_SUBCLASSING_H_
