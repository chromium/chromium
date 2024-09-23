// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/thread/web_thread.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/web_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

class WebThreadTest : public PlatformTest {
 protected:
  static void BasicFunction(base::OnceClosure continuation,
                            WebThread::ID target) {
    auto other_thread = target == WebThread::UI ? WebThread::IO : WebThread::UI;

    EXPECT_TRUE(WebThread::CurrentlyOn(target));
    EXPECT_FALSE(WebThread::CurrentlyOn(other_thread));
    std::move(continuation).Run();
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD};
};

TEST_F(WebThreadTest, BasePostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, GetUITaskRunnerPostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::UI)));
  run_loop.Run();
}

TEST_F(WebThreadTest, GetIOTaskRunnerPostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaTaskRunner) {
  scoped_refptr<base::TaskRunner> task_runner = GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSequencedTaskRunner) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSingleThreadTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

}  // namespace web
