// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator_delegate.h"

// Coordinator for Reading List, displaying the Reading List when starting.
@interface ReadingListCoordinator : ChromeCoordinator

// The delegate handling coordinator dismissal.
@property(nonatomic, weak) id<ReadingListCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_
