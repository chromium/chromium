// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/thread_aware_callback.h"

#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {

namespace {

class TestParameter {
 public:
  TestParameter() : value_(0) {}

  int value_;
};

int called_num = 0;

void TestCallback_0() { ++called_num; }

void TestCallback_1(int p1) { ++called_num; }

void TestCallback_2(int p1, const double* p2) { ++called_num; }

void TestCallback_3(int p1, const double* p2, bool* p3) { ++called_num; }

void TestCallback_4(int p1, const double* p2, bool* p3, TestParameter p4) {
  ++called_num;
}

void TestCallback_5(int p1,
                    const double* p2,
                    bool* p3,
                    TestParameter p4,
                    const TestParameter& p5) {
  ++called_num;
}

typedef proxy::PluginProxyTest ThreadAwareCallbackTest;

// Test that a callback created on the main thread will run on the main thread,
// even when requested from a different thread.
class ThreadAwareCallbackMultiThreadTest
    : public proxy::PluginProxyMultiThreadTest {
 public:
  ThreadAwareCallbackMultiThreadTest() : main_thread_callback_called_(false) {}
  ~ThreadAwareCallbackMultiThreadTest() override {
    CHECK(main_thread_callback_called_);
  }

  // proxy::PluginProxyMultiThreadTest implementation.
  void SetUpTestOnMainThread() override {
    ProxyAutoLock auto_lock;

    main_thread_callback_.reset(
        ThreadAwareCallback<CallbackFunc>::Create(&MainThreadCallbackBody));
  }

  void SetUpTestOnSecondaryThread() override {
    {
      ProxyAutoLock auto_lock;
      main_thread_callback_->RunOnTargetThread(this);
    }

    PostQuitForSecondaryThread();
    PostQuitForMainThread();
  }

 private:
  typedef void (*CallbackFunc)(ThreadAwareCallbackMultiThreadTest*);

  static void MainThreadCallbackBody(ThreadAwareCallbackMultiThreadTest* thiz) {
    thiz->CheckOnThread(MAIN_THREAD);
    thiz->main_thread_callback_called_ = true;

    {
      ProxyAutoLock auto_lock;
      // We have to destroy it prior to the PluginGlobals instance held by the
      // base class. Otherwise it has a ref to Pepper message loop for the main
      // thread and the PluginGlobals destructor will complain.
      thiz->main_thread_callback_.reset(NULL);
    }
  }

  std::unique_ptr<ThreadAwareCallback<CallbackFunc>> main_thread_callback_;
  bool main_thread_callback_called_;
};

// Test that when a ThreadAwareCallback instance is destroyed, pending tasks to
// run the callback will be ignored.
class ThreadAwareCallbackAbortTest : public proxy::PluginProxyMultiThreadTest {
 public:
  ThreadAwareCallbackAbortTest() {}
  ~ThreadAwareCallbackAbortTest() override {}

  // proxy::PluginProxyMultiThreadTest implementation.
  void SetUpTestOnMainThread() override {
    ProxyAutoLock auto_lock;

    main_thread_callback_.reset(
        ThreadAwareCallback<CallbackFunc>::Create(&MainThreadCallbackBody));
  }

  void SetUpTestOnSecondaryThread() override {
    {
      ProxyAutoLock auto_lock;
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&ThreadAwareCallbackAbortTest::DeleteCallback,
                         base::Unretained(this)));
      // |main_thread_callback_| is still valid, even if DeleteCallback() can be
      // called before this following statement. That is because |auto_lock| is
      // still held by this method, which prevents DeleteCallback() from
      // deleting the callback.
      main_thread_callback_->RunOnTargetThread(this);
    }

    PostQuitForSecondaryThread();
    PostQuitForMainThread();
  }

 private:
  typedef void (*CallbackFunc)(ThreadAwareCallbackAbortTest*);

  static void MainThreadCallbackBody(ThreadAwareCallbackAbortTest* thiz) {
    // The callback should not be called.
    ASSERT_TRUE(false);
  }

  void DeleteCallback() {
    ProxyAutoLock auto_lock;
    main_thread_callback_.reset(NULL);
  }

  std::unique_ptr<ThreadAwareCallback<CallbackFunc>> main_thread_callback_;
};

}  // namespace

TEST_F(ThreadAwareCallbackTest, Basics) {
  // ThreadAwareCallback should only be used when the proxy lock has been
  // acquired.
  ProxyAutoLock auto_lock;

  double double_arg = 0.0;
  bool bool_arg = false;
  TestParameter object_arg;

  // Exercise all the template code.
  called_num = 0;
  typedef void (*FuncType_0)();
  std::unique_ptr<ThreadAwareCallback<FuncType_0>> callback_0(
      ThreadAwareCallback<FuncType_0>::Create(TestCallback_0));
  callback_0->RunOnTargetThread();

  typedef void (*FuncType_1)(int);
  std::unique_ptr<ThreadAwareCallback<FuncType_1>> callback_1(
      ThreadAwareCallback<FuncType_1>::Create(TestCallback_1));
  callback_1->RunOnTargetThread(1);

  typedef void (*FuncType_2)(int, const double*);
  std::unique_ptr<ThreadAwareCallback<FuncType_2>> callback_2(
      ThreadAwareCallback<FuncType_2>::Create(TestCallback_2));
  callback_2->RunOnTargetThread(1, &double_arg);

  typedef void (*FuncType_3)(int, const double*, bool*);
  std::unique_ptr<ThreadAwareCallback<FuncType_3>> callback_3(
      ThreadAwareCallback<FuncType_3>::Create(TestCallback_3));
  callback_3->RunOnTargetThread(1, &double_arg, &bool_arg);

  typedef void (*FuncType_4)(int, const double*, bool*, TestParameter);
  std::unique_ptr<ThreadAwareCallback<FuncType_4>> callback_4(
      ThreadAwareCallback<FuncType_4>::Create(TestCallback_4));
  callback_4->RunOnTargetThread(1, &double_arg, &bool_arg, object_arg);

  typedef void (*FuncType_5)(
      int, const double*, bool*, TestParameter, const TestParameter&);
  std::unique_ptr<ThreadAwareCallback<FuncType_5>> callback_5(
      ThreadAwareCallback<FuncType_5>::Create(TestCallback_5));
  callback_5->RunOnTargetThread(
      1, &double_arg, &bool_arg, object_arg, object_arg);

  EXPECT_EQ(6, called_num);
}

TEST_F(ThreadAwareCallbackMultiThreadTest, RunOnTargetThread) { RunTest(); }

TEST_F(ThreadAwareCallbackAbortTest, NotRunIfAborted) { RunTest(); }

}  // namespace ppapi
