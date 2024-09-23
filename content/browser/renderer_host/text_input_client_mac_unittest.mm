// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const int64_t kTaskDelayMs = 200;

// Stub out local frame mojo binding. Intercepts calls for text input
// and marks the message as received. This class attaches to the first
// RenderFrameHostImpl created.
class TextInputClientLocalFrame : public content::FakeLocalFrame,
                                  public WebContentsObserver {
 public:
  explicit TextInputClientLocalFrame(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    if (!initialized_) {
      initialized_ = true;
      Init(render_frame_host->GetRemoteAssociatedInterfaces());
    }
  }

  void GetCharacterIndexAtPoint(const gfx::Point& point) override {
    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  void GetFirstRectForRange(const gfx::Range& range) override {
    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  void SetCallback(base::OnceClosure callback) {
    completion_callback_ = std::move(callback);
  }

 private:
  bool initialized_ = false;
  base::OnceClosure completion_callback_;
};

// This test does not test the WebKit side of the dictionary system (which
// performs the actual data fetching), but rather this just tests that the
// service's signaling system works.
class TextInputClientMacTest : public content::RenderViewHostTestHarness {
 public:
  TextInputClientMacTest() : thread_("TextInputClientMacTestThread") {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    local_frame_ = std::make_unique<TextInputClientLocalFrame>(web_contents());
    RenderViewHostTester::For(rvh())->CreateTestRenderView();
    widget_ = rvh()->GetWidget();
    FocusWebContentsOnMainFrame();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    RenderViewHostTestHarness::TearDown();
  }

  // Accessor for the TextInputClientMac instance.
  TextInputClientMac* service() {
    return TextInputClientMac::GetInstance();
  }

  // Helper method to post a task on the testing thread's MessageLoop after
  // a short delay.
  void PostTask(base::Location from_here, base::OnceClosure task) {
    PostTask(std::move(from_here), std::move(task),
             base::Milliseconds(kTaskDelayMs));
  }

  void PostTask(base::Location from_here,
                base::OnceClosure task,
                const base::TimeDelta delay) {
    thread_.task_runner()->PostDelayedTask(std::move(from_here),
                                           std::move(task), delay);
  }

  RenderWidgetHost* widget() { return widget_; }
  TextInputClientLocalFrame* local_frame() { return local_frame_.get(); }

  IPC::TestSink& ipc_sink() {
    return static_cast<MockRenderProcessHost*>(widget()->GetProcess())->sink();
  }

 private:
  friend class ScopedTestingThread;

  raw_ptr<RenderWidgetHost, DanglingUntriaged> widget_;
  std::unique_ptr<TextInputClientLocalFrame> local_frame_;

  base::Thread thread_;
};

////////////////////////////////////////////////////////////////////////////////

// Helper class that Start()s and Stop()s a thread according to the scope of the
// object.
class ScopedTestingThread {
 public:
  ScopedTestingThread(TextInputClientMacTest* test) : thread_(test->thread_) {
    thread_->Start();
  }
  ~ScopedTestingThread() { thread_->Stop(); }

 private:
  const raw_ref<base::Thread> thread_;
};

}  // namespace

// Test Cases //////////////////////////////////////////////////////////////////

TEST_F(TextInputClientMacTest, GetCharacterIndex) {
  ScopedTestingThread thread(this);
  const NSUInteger kSuccessValue = 42;

  PostTask(FROM_HERE,
           base::BindOnce(&TextInputClientMac::SetCharacterIndexAndSignal,
                          base::Unretained(service()), kSuccessValue));
  base::RunLoop run_loop;
  local_frame()->SetCallback(run_loop.QuitClosure());
  NSUInteger index = service()->GetCharacterIndexAtPoint(
      widget(), gfx::Point(2, 2));

  EXPECT_EQ(kSuccessValue, index);
  run_loop.Run();
}

TEST_F(TextInputClientMacTest, TimeoutCharacterIndex) {
  base::RunLoop run_loop;
  local_frame()->SetCallback(run_loop.QuitClosure());
  uint32_t index =
      service()->GetCharacterIndexAtPoint(widget(), gfx::Point(2, 2));

  EXPECT_EQ(UINT32_MAX, index);
  run_loop.Run();
}

TEST_F(TextInputClientMacTest, NotFoundCharacterIndex) {
  ScopedTestingThread thread(this);
  const NSUInteger kPreviousValue = 42;

  // Set an arbitrary value to ensure the index is not |NSNotFound|.
  PostTask(FROM_HERE,
           base::BindOnce(&TextInputClientMac::SetCharacterIndexAndSignal,
                          base::Unretained(service()), kPreviousValue));

  // Set UINT32_MAX to the index |kTaskDelayMs| after the previous setting.
  PostTask(FROM_HERE,
           base::BindOnce(&TextInputClientMac::SetCharacterIndexAndSignal,
                          base::Unretained(service()), UINT32_MAX),
           base::Milliseconds(kTaskDelayMs) * 2);

  base::RunLoop run_loop1;
  local_frame()->SetCallback(run_loop1.QuitClosure());
  uint32_t index =
      service()->GetCharacterIndexAtPoint(widget(), gfx::Point(2, 2));
  run_loop1.Run();
  EXPECT_EQ(kPreviousValue, index);

  base::RunLoop run_loop2;
  local_frame()->SetCallback(run_loop2.QuitClosure());
  index = service()->GetCharacterIndexAtPoint(widget(), gfx::Point(2, 2));
  run_loop2.Run();
  EXPECT_EQ(UINT32_MAX, index);
}

TEST_F(TextInputClientMacTest, GetRectForRange) {
  ScopedTestingThread thread(this);
  const gfx::Rect kSuccessValue(42, 43, 44, 45);

  PostTask(FROM_HERE,
           base::BindOnce(&TextInputClientMac::SetFirstRectAndSignal,
                          base::Unretained(service()), kSuccessValue));
  base::RunLoop run_loop;
  local_frame()->SetCallback(run_loop.QuitClosure());
  gfx::Rect rect =
      service()->GetFirstRectForRange(widget(), gfx::Range(NSMakeRange(0, 32)));
  run_loop.Run();
  EXPECT_EQ(kSuccessValue, rect);
}

TEST_F(TextInputClientMacTest, TimeoutRectForRange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kTextInputClient,
                                                  {{"ipc_timeout", "300ms"}});

  base::RunLoop run_loop;
  local_frame()->SetCallback(run_loop.QuitClosure());

  gfx::Rect rect =
      service()->GetFirstRectForRange(widget(), gfx::Range(NSMakeRange(0, 32)));
  run_loop.Run();

  EXPECT_EQ(gfx::Rect(), rect);
}

}  // namespace content
