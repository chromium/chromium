// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/render_frame_impl.h"
#include "content/shell/browser/shell.h"

namespace content {

class CommitObserver : public RenderViewObserver {
 public:
  CommitObserver(RenderView* render_view)
      : RenderViewObserver(render_view), quit_closures_(), commit_count_(0) {}

  void DidCommitCompositorFrame() override {
    commit_count_++;
    for (const auto& pair : quit_closures_) {
      pair.second.Run();
    }
  }

  void QuitAfterCommit(int commit_number,
                       scoped_refptr<MessageLoopRunner> runner) {
    if (commit_number >= commit_count_) runner->Quit();
  }

  void WaitForCommitNumber(int commit_number) {
    if (commit_number > commit_count_) {
      scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
      quit_closures_[commit_number] =
          base::BindRepeating(&CommitObserver::QuitAfterCommit,
                              base::Unretained(this), commit_number, runner);
      runner->Run();
      quit_closures_.erase(commit_number);
    }
  }

  int GetCommitCount() { return commit_count_; }

 private:
  // RenderViewObserver implementation.
  void OnDestruct() override { delete this; }

  base::flat_map<int, base::RepeatingClosure> quit_closures_;
  int commit_count_;
};

class VisualStateTest : public ContentBrowserTest {
 public:
  VisualStateTest() : callback_count_(0) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void WaitForCommit(CommitObserver *observer, int commit_number) {
    observer->WaitForCommitNumber(commit_number);
    EXPECT_EQ(commit_number, observer->GetCommitCount());
  }

  void AssertIsIdle() {
    ASSERT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  }

  void InvokeVisualStateCallback(bool result) {
    EXPECT_TRUE(result);
    callback_count_++;
  }

  int GetCallbackCount() { return callback_count_; }

 private:
  int callback_count_;
};

// This test verifies that visual state callbacks do not deadlock. In other
// words, the visual state callback should be received even if there are no
// pending updates or commits.
// Disabled due to cross-platform flakes; http://crbug.com/462580.
IN_PROC_BROWSER_TEST_F(VisualStateTest, DISABLED_CallbackDoesNotDeadlock) {
  // This test relies on the fact that loading "about:blank" only requires a
  // single commit. We first load "about:blank" and wait for this single
  // commit. At that point we know that the page has stabilized and no
  // further commits are expected. We then insert a visual state callback
  // and verify that this causes an additional commit in order to deliver
  // the callback.
  // Unfortunately, if loading "about:blank" changes and starts requiring
  // two commits then this test will prove nothing. We could detect this
  // with a high level of confidence if we used a timeout, but that's
  // discouraged (see https://codereview.chromium.org/939673002).
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  CommitObserver observer(RenderView::FromRoutingID(
      shell()->web_contents()->GetRenderViewHost()->GetRoutingID()));

  // Wait for the commit corresponding to the load.

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &VisualStateTest::WaitForCommit, base::Unretained(this), &observer, 1));

  // Try our best to check that there are no pending updates or commits.
  PostTaskToInProcessRendererAndWait(
      base::BindOnce(&VisualStateTest::AssertIsIdle, base::Unretained(this)));

  // Insert a visual state callback.
  shell()->web_contents()->GetMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&VisualStateTest::InvokeVisualStateCallback,
                     base::Unretained(this)));

  // Verify that the callback is invoked and a new commit completed.
  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &VisualStateTest::WaitForCommit, base::Unretained(this), &observer, 2));
  EXPECT_EQ(1, GetCallbackCount());
}

}  // namespace content
