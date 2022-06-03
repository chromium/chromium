// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/thread/web_thread.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/web_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

class WebThreadTest : public PlatformTest {
 protected:
  static void BasicFunction(base::OnceClosure continuation,
                            WebThread::ID target) {
    EXPECT_TRUE(WebThread::CurrentlyOn(target));
    std::move(continuation).Run();
  }

  web::WebTaskEnvironment task_environment_;
};

TEST_F(WebThreadTest, PostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(base::PostTask(
      FROM_HERE, {WebThread::IO},
      base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                     WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaTaskRunner) {
  scoped_refptr<base::TaskRunner> task_runner =
      base::CreateTaskRunner({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSequencedTaskRunner) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSingleThreadTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunner({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

}  // namespace web
