// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/sandboxed_process_thread_type_handler.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_type.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "content/common/thread_type_switcher.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Fake browser-side ThreadTypeSwitcher that records every batched call.
class FakeThreadTypeSwitcher : public mojom::ThreadTypeSwitcher {
 public:
  void SetThreadTypes(
      std::vector<mojom::ThreadTypeChangePtr> changes) override {
    ++call_count_;
    for (const auto& change : changes) {
      received_.emplace_back(change->platform_thread_id, change->thread_type);
    }
  }

  int call_count() const { return call_count_; }
  const std::vector<std::pair<int32_t, base::ThreadType>>& received() const {
    return received_;
  }

 private:
  int call_count_ = 0;
  std::vector<std::pair<int32_t, base::ThreadType>> received_;
};

// Runs a closure on a thread that has no task runner.
class ClosureThreadDelegate : public base::DelegateSimpleThread::Delegate {
 public:
  explicit ClosureThreadDelegate(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  void Run() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;
};

}  // namespace

class SandboxedProcessThreadTypeHandlerTest : public ::testing::Test {
 protected:
  SandboxedProcessThreadTypeHandlerTest() {
    CreateHandler();
    BindFakeSwitcher();
  }

  // Creates `handler_` on the calling thread.
  void CreateHandler() {
    handler_ = base::WrapUnique(new SandboxedProcessThreadTypeHandler());
    // The constructor registers `handler_` as the process-wide thread type
    // delegate. Detach it so real thread type changes don't interfere; the
    // test drives HandleThreadTypeChange() directly.
    base::PlatformThread::SetThreadTypeDelegate(nullptr);
  }

  // Replaces `handler_` with one created on a thread that has no task runner,
  // like the renderer main thread before its scheduler is created.
  void RecreateHandlerOnThreadWithoutTaskRunner() {
    fake_receiver_.reset();
    handler_.reset();
    ClosureThreadDelegate delegate(
        base::BindOnce(&SandboxedProcessThreadTypeHandlerTest::CreateHandler,
                       base::Unretained(this)));
    base::DelegateSimpleThread thread(&delegate, "NoTaskRunner");
    thread.Start();
    thread.Join();
    BindFakeSwitcher();
  }

  void BindFakeSwitcher() {
    fake_receiver_.emplace(&fake_, handler_->PassReceiverForTesting());
  }

  void Change(int32_t tid, base::ThreadType type) {
    handler_->HandleThreadTypeChange(base::PlatformThreadId(tid), type);
  }

  void Connect() {
    handler_->OnSwitcherConnected(task_environment_.GetMainThreadTaskRunner());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeThreadTypeSwitcher fake_;
  std::unique_ptr<SandboxedProcessThreadTypeHandler> handler_;
  std::optional<mojo::Receiver<mojom::ThreadTypeSwitcher>> fake_receiver_;
};

// Changes that arrive before the switcher is connected accumulate and are
// flushed as a single batched IPC, with later changes to the same thread
// replacing earlier ones.
TEST_F(SandboxedProcessThreadTypeHandlerTest, CoalescesStartupBurst) {
  Change(11, base::ThreadType::kDefault);
  Change(12, base::ThreadType::kBackground);
  Change(11, base::ThreadType::kUtility);  // Replaces the earlier tid 11 entry.

  // Nothing is posted before the switcher is connected, so nothing is sent.
  EXPECT_EQ(fake_.call_count(), 0);

  Connect();
  // The accumulated changes are flushed as a single batched IPC carrying the
  // two distinct (deduplicated) threads.
  ASSERT_TRUE(base::test::RunUntil([&] { return fake_.call_count() == 1; }));
  ASSERT_EQ(fake_.received().size(), 2u);
  std::map<int32_t, base::ThreadType> got(fake_.received().begin(),
                                          fake_.received().end());
  EXPECT_EQ(got[11], base::ThreadType::kUtility);
  EXPECT_EQ(got[12], base::ThreadType::kBackground);
}

// Multiple changes posted while connected coalesce into one IPC per flush.
TEST_F(SandboxedProcessThreadTypeHandlerTest, CoalescesWhileConnected) {
  Connect();
  Change(21, base::ThreadType::kDefault);
  Change(22, base::ThreadType::kBackground);

  ASSERT_TRUE(base::test::RunUntil([&] { return fake_.call_count() == 1; }));
  EXPECT_EQ(fake_.received().size(), 2u);
  std::map<int32_t, base::ThreadType> got(fake_.received().begin(),
                                          fake_.received().end());
  EXPECT_EQ(got[21], base::ThreadType::kDefault);
  EXPECT_EQ(got[22], base::ThreadType::kBackground);
}

// The handler can be created before any task runner exists, and changes made
// from then on are flushed once connected. The renderer depends on this to
// create the handler before its main thread scheduler.
TEST_F(SandboxedProcessThreadTypeHandlerTest, CreatedWithoutTaskRunner) {
  RecreateHandlerOnThreadWithoutTaskRunner();
  Change(31, base::ThreadType::kPresentation);
  Connect();

  ASSERT_TRUE(base::test::RunUntil([&] { return fake_.call_count() == 1; }));
  ASSERT_EQ(fake_.received().size(), 1u);
  EXPECT_EQ(fake_.received()[0].first, 31);
  EXPECT_EQ(fake_.received()[0].second, base::ThreadType::kPresentation);
}

}  // namespace content
