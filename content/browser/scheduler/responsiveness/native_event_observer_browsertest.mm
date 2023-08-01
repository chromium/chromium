// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "ui/events/test/cocoa_test_event_utils.h"

#import <Carbon/Carbon.h>

namespace content::responsiveness {

namespace {

class FakeNativeEventObserver : public NativeEventObserver {
 public:
  FakeNativeEventObserver()
      : NativeEventObserver(base::DoNothing(), base::DoNothing()) {}
  ~FakeNativeEventObserver() override = default;

  void WillRunNativeEvent(const void* opaque_identifier) override {
    ASSERT_FALSE(will_run_id_);
    will_run_id_ = opaque_identifier;
  }
  void DidRunNativeEvent(const void* opaque_identifier) override {
    ASSERT_FALSE(did_run_id_);
    did_run_id_ = opaque_identifier;
  }

  const void* will_run_id() { return will_run_id_; }
  const void* did_run_id() { return did_run_id_; }

 private:
  raw_ptr<const void> will_run_id_ = nullptr;
  raw_ptr<const void> did_run_id_ = nullptr;
};

}  // namespace

class ResponsivenessNativeEventObserverBrowserTest : public ContentBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ResponsivenessNativeEventObserverBrowserTest,
                       EventForwarding) {
  FakeNativeEventObserver observer;

  EXPECT_FALSE(observer.will_run_id());
  EXPECT_FALSE(observer.did_run_id());
  NSEvent* event = cocoa_test_event_utils::KeyEventWithKeyCode(
      kVK_Return, '\r', NSEventTypeKeyDown, 0);
  [NSApp sendEvent:event];

  EXPECT_EQ(observer.will_run_id(), (__bridge void*)event);
  EXPECT_EQ(observer.did_run_id(), (__bridge void*)event);
}

}  // namespace content::responsiveness
