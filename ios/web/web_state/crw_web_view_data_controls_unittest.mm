// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/metrics/histogram_tester.h"
#import "ios/components/enterprise/data_controls/clipboard_enums.h"
#import "ios/web/web_state/crw_data_controls_delegate.h"
#import "ios/web/web_state/crw_web_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// A fake data controls delegate for testing purposes.
@interface FakeDataControlsDelegate : NSObject <CRWDataControlsDelegate>
@property(nonatomic, assign) BOOL shouldAllowCopy;
@property(nonatomic, assign) BOOL shouldAllowPaste;
@property(nonatomic, assign) BOOL shouldAllowCut;
@end

@implementation FakeDataControlsDelegate

- (instancetype)init {
  if ((self = [super init])) {
    _shouldAllowCopy = YES;
    _shouldAllowPaste = YES;
    _shouldAllowCut = YES;
  }
  return self;
}

- (void)shouldAllowCopyWithDecisionHandler:(void (^)(BOOL))decisionHandler {
  decisionHandler(self.shouldAllowCopy);
}

- (void)shouldAllowPasteWithDecisionHandler:(void (^)(BOOL))decisionHandler {
  decisionHandler(self.shouldAllowPaste);
}

- (void)shouldAllowCutWithDecisionHandler:(void (^)(BOOL))decisionHandler {
  decisionHandler(self.shouldAllowCut);
}

@end

namespace {

// Test fixture for CRWWebView data controls integration.
class CRWWebViewDataControlsTest : public PlatformTest {
 protected:
  CRWWebViewDataControlsTest() {
    delegate_ = [[FakeDataControlsDelegate alloc] init];
    web_view_ = [[CRWWebView alloc] init];
    web_view_.dataControlsDelegate = delegate_;
  }

  CRWWebView* web_view() { return web_view_; }

  FakeDataControlsDelegate* delegate_;
  CRWWebView* web_view_;
  base::HistogramTester histogram_tester_;
};

// Tests that copy is allowed and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, CopyAllowed) {
  delegate_.shouldAllowCopy = YES;
  [web_view() copy:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Copy.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Copy.Outcome",
                                       true, 1);
}

// Tests that copy is denied and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, CopyDenied) {
  delegate_.shouldAllowCopy = NO;
  [web_view() copy:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Copy.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Copy.Outcome",
                                       false, 1);
}

// Tests that paste is allowed and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, PasteAllowed) {
  delegate_.shouldAllowPaste = YES;
  [web_view() paste:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Paste.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Paste.Outcome",
                                       true, 1);
}

// Tests that paste is denied and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, PasteDenied) {
  delegate_.shouldAllowPaste = NO;
  [web_view() paste:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Paste.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Paste.Outcome",
                                       false, 1);
}

// Tests that cut is allowed and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, CutAllowed) {
  delegate_.shouldAllowCut = YES;
  [web_view() cut:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Cut.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Cut.Outcome",
                                       true, 1);
}

// Tests that cut is denied and metrics are recorded.
TEST_F(CRWWebViewDataControlsTest, CutDenied) {
  delegate_.shouldAllowCut = NO;
  [web_view() cut:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Cut.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Cut.Outcome",
                                       false, 1);
}

// Tests that metrics are recorded when there is no delegate.
TEST_F(CRWWebViewDataControlsTest, NoDelegate) {
  web_view_.dataControlsDelegate = nil;
  [web_view() copy:nil];
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Copy.Source",
      static_cast<int>(data_controls::ClipboardSource::kEditMenu), 1);
  histogram_tester_.ExpectTotalCount("IOS.WebState.Clipboard.Copy.Outcome", 0);
}

}  // namespace
