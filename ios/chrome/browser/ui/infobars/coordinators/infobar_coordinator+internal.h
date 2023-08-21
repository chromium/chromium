// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_INTERNAL_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_INTERNAL_H_

// Extension to provide access for unit tests to easily inject state.
@interface InfobarCoordinator (Testing)

- (base::TimeDelta)infobarPresentationDuration;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_INTERNAL_H_
