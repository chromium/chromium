// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_thread_type_switcher_linux.h"

#include <errno.h>
#include <sys/resource.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_type.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "content/common/thread_type_switcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// A thread that reports its id and then parks until released.
class ParkedThread : public base::DelegateSimpleThread::Delegate {
 public:
  ParkedThread() : thread_(this, "ParkedThread") { thread_.StartAsync(); }
  ~ParkedThread() override { Join(); }

  base::PlatformThreadId tid() {
    started_.Wait();
    return tid_;
  }

  void Join() {
    if (!joined_) {
      release_.Signal();
      thread_.Join();
      joined_ = true;
    }
  }

 private:
  void Run() override {
    tid_ = base::PlatformThread::CurrentId();
    started_.Signal();
    release_.Wait();
  }

  base::DelegateSimpleThread thread_;
  base::PlatformThreadId tid_;
  base::WaitableEvent started_;
  base::WaitableEvent release_;
  bool joined_ = false;
};

// Applies the changes on the test's main thread instead of the process
// launcher task runner.
class TestChildThreadTypeSwitcher : public ChildThreadTypeSwitcher {
 protected:
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
};

class ChildThreadTypeSwitcherTest : public testing::Test {
 protected:
  void SetUp() override {
    switcher_ = std::make_unique<TestChildThreadTypeSwitcher>();
    // The "child" is this process: seen from inside a process, a thread's id in
    // its own PID namespace is just its id, which is what a real child sends.
    switcher_->SetPid(getpid());
    ASSERT_TRUE(switcher_->Bind(remote_.BindNewPipeAndPassReceiver()));
  }

  void TearDown() override {
    remote_.reset();
    switcher_.reset();
    // The switcher destroys its sequence-bound state asynchronously.
    FlushTaskRunner();
  }

  // Runs everything queued on the task runner the switcher was given.
  void FlushTaskRunner() {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void SetThreadType(base::PlatformThreadId tid, base::ThreadType type) {
    std::vector<mojom::ThreadTypeChangePtr> changes;
    changes.push_back(mojom::ThreadTypeChange::New(tid.raw(), type));
    remote_->SetThreadTypes(std::move(changes));
    // The switcher posts the work to the same task runner while handling the
    // message, so it has run by the time the flush reply is dispatched.
    remote_.FlushForTesting();
  }

  // getpriority() returns the nice value: larger means lower priority.
  static int GetNiceValue(base::PlatformThreadId tid) {
    errno = 0;
    const int nice_value = getpriority(PRIO_PROCESS, tid.raw());
    EXPECT_EQ(errno, 0);
    return nice_value;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChildThreadTypeSwitcher> switcher_;
  mojo::Remote<mojom::ThreadTypeSwitcher> remote_;
};

TEST_F(ChildThreadTypeSwitcherTest, AppliesToTheNamedThread) {
  ParkedThread thread;
  const base::PlatformThreadId tid = thread.tid();
  const int nice_before = GetNiceValue(tid);
  const int own_nice_before = GetNiceValue(base::PlatformThread::CurrentId());

  // Lowering a priority needs no privilege, so kBackground is observable.
  SetThreadType(tid, base::ThreadType::kBackground);
  const int nice_background = GetNiceValue(tid);
  EXPECT_GT(nice_background, nice_before);
  // Only that thread changed.
  EXPECT_EQ(GetNiceValue(base::PlatformThread::CurrentId()), own_nice_before);

  // A second change for the same, now known, thread takes the validated
  // cache path.
  SetThreadType(tid, base::ThreadType::kBackground);
  EXPECT_EQ(GetNiceValue(tid), nice_background);
}

TEST_F(ChildThreadTypeSwitcherTest, ThreadsThatComeAndGo) {
  {
    ParkedThread first;
    const int nice_before = GetNiceValue(first.tid());
    SetThreadType(first.tid(), base::ThreadType::kBackground);
    EXPECT_GT(GetNiceValue(first.tid()), nice_before);
  }
  // A thread created after the switcher last scanned the process.
  ParkedThread second;
  const int nice_before = GetNiceValue(second.tid());
  SetThreadType(second.tid(), base::ThreadType::kBackground);
  EXPECT_GT(GetNiceValue(second.tid()), nice_before);
  // An id that names no thread of this process is ignored.
  SetThreadType(base::PlatformThreadId(1), base::ThreadType::kBackground);
}

}  // namespace
}  // namespace content
