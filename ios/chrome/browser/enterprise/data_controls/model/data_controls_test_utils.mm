// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_test_utils.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager_observer.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Scoped observer for `DataControlsPasteboardManager` that executes a callback
// when `OnPasteboardContentChanged()` is received.
class ScopedPasteboardContentObserver
    : public data_controls::DataControlsPasteboardManagerObserver {
 public:
  explicit ScopedPasteboardContentObserver(base::OnceClosure callback)
      : callback_(std::move(callback)), observation_(this) {
    observation_.Observe(
        data_controls::DataControlsPasteboardManager::GetInstance());
  }
  ~ScopedPasteboardContentObserver() override = default;

  // DataControlsPasteboardManagerObserver:
  void OnPasteboardContentChanged() override {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  base::OnceClosure callback_;
  base::ScopedObservation<data_controls::DataControlsPasteboardManager,
                          data_controls::DataControlsPasteboardManagerObserver>
      observation_;
};

}  // namespace

void SetCopyBlockRule(PrefService* prefs) {
  data_controls::SetDataControls(prefs, {R"({
                        "sources": {
                          "urls": ["https://block.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                                 /*machine_scope=*/false);
}

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

bool WaitForPasteboardContentChanged(base::OnceClosure action) {
  base::test::TestFuture<void> future;
  ScopedPasteboardContentObserver observer(future.GetCallback());
  std::move(action).Run();
  return future.Wait();
}
