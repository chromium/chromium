// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ContextualSheetPresenter;

// Coordinator for a custom sheet view to display the Contextual Panel.
@interface ContextualSheetCoordinator : ChromeCoordinator

// Preesentation delegate for this sheet.
@property(nonatomic, weak) id<ContextualSheetPresenter> presenter;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_COORDINATOR_H_
