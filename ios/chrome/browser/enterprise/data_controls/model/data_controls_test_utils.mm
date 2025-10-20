// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_test_utils.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

bool WaitForKnownPasteboardSource() {
  return WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return data_controls::DataControlsPasteboardManager::GetInstance()
            ->GetCurrentPasteboardItemsSource()
            .source_profile;
      });
}

bool WaitForUnknownPasteboardSource() {
  return WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return data_controls::DataControlsPasteboardManager::GetInstance()
            ->GetCurrentPasteboardItemsSource()
            .source_url.is_empty();
      });
}

bool WaitForStringInPasteboard(NSString* expected_string) {
  return WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return [UIPasteboard.generalPasteboard.string
            isEqualToString:expected_string];
      });
}
