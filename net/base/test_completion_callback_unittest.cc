// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Illustrates how to use net::TestCompletionCallback.

#include "net/base/test_completion_callback.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const int kMagicResult = 8888;

void CallClosureAfterCheckingResult(base::OnceClosure closure,
                                    bool* did_check_result,
                                    int result) {
  DCHECK_EQ(result, kMagicResult);
  *did_check_result = true;
  std::move(closure).Run();
}

// ExampleEmployer is a toy version of HostResolver
// TODO: restore damage done in extracting example from real code
// (e.g. bring back real destructor, bring back comments)
class ExampleEmployer {
 public:
  ExampleEmployer();
  ExampleEmployer(const ExampleEmployer&) = delete;
  ExampleEmployer& operator=(const ExampleEmployer&) = delete;
  ~ExampleEmployer();

  // Posts to the current thread a task which itself posts |callback| to the
  // current thread. Returns true on success
  bool DoSomething(CompletionOnceCallback callback);

 private:
  class ExampleWorker;
  friend class ExampleWorker;
  scoped_refptr<ExampleWorker> request_;
};

// Helper class; this is how ExampleEmployer schedules work.
class ExampleEmployer::ExampleWorker
    : public base::RefCountedThreadSafe<ExampleWorker> {
 public:
  ExampleWorker(ExampleEmployer* employer, CompletionOnceCallback callback)
      : employer_(employer), callback_(std::move(callback)) {}
  void DoWork();
  void DoCallback();
 private:
  friend class base::RefCountedThreadSafe<ExampleWorker>;

  ~ExampleWorker() = default;

  // Only used on the origin thread (where DoSomething was called).
  raw_ptr<ExampleEmployer> employer_;
  CompletionOnceCallback callback_;
  // Used to post ourselves onto the origin thread.
  const scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
};

void ExampleEmployer::ExampleWorker::DoWork() {
  // In a real worker thread, some work would be done here.
  // Pretend it is, and send the completion callback.
  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExampleWorker::DoCallback, this));
}

void ExampleEmployer::ExampleWorker::DoCallback() {
  // Running on the origin thread.

  // Drop the employer_'s reference to us.  Do this before running the
  // callback since the callback might result in the employer being
  // destroyed.
  employer_->request_ = nullptr;

  std::move(callback_).Run(kMagicResult);
}

ExampleEmployer::ExampleEmployer() = default;

ExampleEmployer::~ExampleEmployer() = default;

bool ExampleEmployer::DoSomething(CompletionOnceCallback callback) {
  DCHECK(!request_.get()) << "already in use";

  request_ = base::MakeRefCounted<ExampleWorker>(this, std::move(callback));

  if (!base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ExampleWorker::DoWork, request_))) {
    NOTREACHED_IN_MIGRATION();
    request_ = nullptr;
    return false;
  }

  return true;
}

}  // namespace

class TestCompletionCallbackTest : public PlatformTest,
                                   public WithTaskEnvironment {};

TEST_F(TestCompletionCallbackTest, Simple) {
  ExampleEmployer boss;
  TestCompletionCallback callback;
  bool queued = boss.DoSomething(callback.callback());
  EXPECT_TRUE(queued);
  int result = callback.WaitForResult();
  EXPECT_EQ(result, kMagicResult);
}

TEST_F(TestCompletionCallbackTest, Closure) {
  ExampleEmployer boss;
  TestClosure closure;
  bool did_check_result = false;
  CompletionOnceCallback completion_callback =
      base::BindOnce(&CallClosureAfterCheckingResult, closure.closure(),
                     base::Unretained(&did_check_result));
  bool queued = boss.DoSomething(std::move(completion_callback));
  EXPECT_TRUE(queued);

  EXPECT_FALSE(did_check_result);
  closure.WaitForResult();
  EXPECT_TRUE(did_check_result);
}

// TODO: test deleting ExampleEmployer while work outstanding

}  // namespace net
