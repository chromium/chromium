// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/browser_interface_binders.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"

namespace content {

namespace {

class VibrationObserver : public WebContentsObserver {
 public:
  explicit VibrationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void VibrationRequested() override {
    did_vibrate_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (!did_vibrate_) {
      run_loop_.Run();
    }
  }

  bool DidVibrate() { return did_vibrate_; }

 private:
  bool did_vibrate_ = false;
  base::RunLoop run_loop_;
};

class VibrationTest : public ContentBrowserTest,
                      public device::mojom::VibrationManager {
 public:
  VibrationTest() {
    OverrideVibrationManagerBinderForTesting(base::BindRepeating(
        &VibrationTest::BindVibrationManager, base::Unretained(this)));
  }

  VibrationTest(const VibrationTest&) = delete;
  VibrationTest& operator=(const VibrationTest&) = delete;

  ~VibrationTest() override {
    OverrideVibrationManagerBinderForTesting(base::NullCallback());
  }

  void BindVibrationManager(
      mojo::PendingReceiver<device::mojom::VibrationManager> receiver,
      mojo::PendingRemote<device::mojom::VibrationManagerListener> listener) {
    receiver_.Bind(std::move(receiver));
    listener_.Bind(std::move(listener));
  }

 protected:
  void TriggerVibrate(int duration, base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);

    RenderFrameHost* frame = shell()->web_contents()->GetPrimaryMainFrame();
    std::string script =
        "navigator.vibrate(" + base::NumberToString(duration) + ")";
    EXPECT_TRUE(ExecJs(frame, script));
  }

  int64_t vibrate_milliseconds() { return vibrate_milliseconds_; }

 private:
  // device::mojom::VibrationManager:
  void Vibrate(int64_t milliseconds, VibrateCallback callback) override {
    vibrate_milliseconds_ = milliseconds;
    std::move(callback).Run();
    std::move(vibrate_done_).Run();
    listener_->OnVibrate();
  }
  void Cancel(CancelCallback callback) override { std::move(callback).Run(); }

  int64_t vibrate_milliseconds_ = -1;
  base::OnceClosure vibrate_done_;
  mojo::Receiver<device::mojom::VibrationManager> receiver_{this};
  mojo::Remote<device::mojom::VibrationManagerListener> listener_;
};

IN_PROC_BROWSER_TEST_F(VibrationTest, Vibrate) {
  ASSERT_EQ(-1, vibrate_milliseconds());

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  base::RunLoop run_loop;
  TriggerVibrate(1234, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(1234, vibrate_milliseconds());
}

IN_PROC_BROWSER_TEST_F(VibrationTest, VibrateNotifiesListener) {
  VibrationObserver observer(shell()->web_contents());
  EXPECT_FALSE(observer.DidVibrate());

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  base::RunLoop run_loop;

  TriggerVibrate(1234, run_loop.QuitClosure());
  run_loop.Run();
  observer.Wait();
  EXPECT_TRUE(observer.DidVibrate());
}

}  //  namespace

}  //  namespace content
