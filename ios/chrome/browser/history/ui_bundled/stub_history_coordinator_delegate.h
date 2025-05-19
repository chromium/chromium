// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_STUB_HISTORY_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_STUB_HISTORY_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"

// A class used as a test stub for `HistoryCoordinatorDelegate`.
@interface StubHistoryCoordinatorDelegate
    : NSObject <HistoryCoordinatorDelegate>
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_STUB_HISTORY_COORDINATOR_DELEGATE_H_
