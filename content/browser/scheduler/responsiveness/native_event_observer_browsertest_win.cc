// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/win/message_window.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"

namespace content {
namespace responsiveness {

namespace {

bool HandleMessage(UINT message,
                   WPARAM wparam,
                   LPARAM lparam,
                   LRESULT* result) {
  return false;
}

}  // namespace

class ResponsivenessNativeEventObserverBrowserTest : public ContentBrowserTest {
 public:
  void WillRunEvent(const void* opaque_id) {
    ASSERT_FALSE(will_run_id_);
    will_run_id_ = opaque_id;
  }
  void DidRunEvent(const void* opaque_id) {
    ASSERT_FALSE(did_run_id_);
    did_run_id_ = opaque_id;
    std::move(quit_closure_).Run();
  }

 protected:
  raw_ptr<const void> will_run_id_ = nullptr;
  raw_ptr<const void> did_run_id_ = nullptr;
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ResponsivenessNativeEventObserverBrowserTest,
                       EventForwarding) {
  base::win::MessageWindow window;
  EXPECT_TRUE(window.Create(base::BindRepeating(&HandleMessage)));

  NativeEventObserver observer(
      base::BindRepeating(
          &ResponsivenessNativeEventObserverBrowserTest::WillRunEvent,
          base::Unretained(this)),
      base::BindRepeating(
          &ResponsivenessNativeEventObserverBrowserTest::DidRunEvent,
          base::Unretained(this)));

  EXPECT_FALSE(will_run_id_);
  EXPECT_FALSE(did_run_id_);

  EXPECT_NE(PostMessage(window.hwnd(), WM_USER, 100, 0), 0);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  EXPECT_EQ(will_run_id_, did_run_id_);
  EXPECT_NE(will_run_id_, nullptr);
}

}  // namespace responsiveness
}  // namespace content
