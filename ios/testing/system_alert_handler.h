// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_SYSTEM_ALERT_HANDLER_H_
#define IOS_TESTING_SYSTEM_ALERT_HANDLER_H_

#import <Foundation/Foundation.h>

// A mapping between alert texts (partial match) and corresponding button titles
// (exact match) to dismiss the known system alerts.
NSDictionary<NSString*, NSArray<NSString*>*>* TextToButtonsOfKnownSystemAlerts(
    void);

// Closes system alerts from the `TextToButtonsOfKnownSystemAlerts` text/button
// mapping.
BOOL HandleKnownSystemAlertsIfVisible(void);

#endif  // IOS_TESTING_SYSTEM_ALERT_HANDLER_H_
