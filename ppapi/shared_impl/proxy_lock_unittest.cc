// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {

namespace {

bool expect_to_be_locked = false;
void CheckLockState() {
  if (expect_to_be_locked) {
    ProxyLock::AssertAcquired();
  } else {
    // If we expect to be unlocked, try to lock. We rely on the checking inside
    // base::Lock that prevents recursive locking.
    ProxyAutoLock lock;
  }
}

int called_num = 0;

class CheckLockStateInDestructor
    : public base::RefCounted<CheckLockStateInDestructor> {
 public:
  CheckLockStateInDestructor() {}

  CheckLockStateInDestructor(const CheckLockStateInDestructor&) = delete;
  CheckLockStateInDestructor& operator=(const CheckLockStateInDestructor&) =
      delete;

  void Method() { ++called_num; }

 private:
  friend class base::RefCounted<CheckLockStateInDestructor>;
  ~CheckLockStateInDestructor() { CheckLockState(); }
};

void TestCallback_0() {
  CheckLockState();
  ++called_num;
}

void TestCallback_1(int p1) {
  CheckLockState();
  ++called_num;
}

void TestCallback_2(int p1, const std::string& p2) {
  CheckLockState();
  ++called_num;
}

struct Param {};
void TestCallback_3(int p1, const std::string& p2, Param p3) {
  CheckLockState();
  ++called_num;
}

}  // namespace

class PpapiProxyLockTest : public testing::Test {
  base::test::SingleThreadTaskEnvironment
      task_environment_;  // Required to receive callbacks.
};

TEST_F(PpapiProxyLockTest, Locking) {
  TestGlobals globals;
  expect_to_be_locked = true;

  base::OnceCallback<void()> cb0;
  {
    ProxyAutoLock lock;
    cb0 = RunWhileLocked(base::BindOnce(TestCallback_0));
  }
  std::move(cb0).Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    cb0 = RunWhileLocked(base::BindOnce(TestCallback_1, 123));
  }
  std::move(cb0).Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    scoped_refptr<CheckLockStateInDestructor> object =
        new CheckLockStateInDestructor();
    cb0 = RunWhileLocked(
        base::BindOnce(&CheckLockStateInDestructor::Method, object));
    // Note after this scope, the Callback owns the only reference.
  }
  std::move(cb0).Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::OnceCallback<void(int)> cb1;
  {
    ProxyAutoLock lock;
    cb1 = RunWhileLocked(base::BindOnce(TestCallback_1));
  }
  std::move(cb1).Run(123);
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::OnceCallback<void(int, const std::string&)> cb2;
  {
    ProxyAutoLock lock;
    cb2 = RunWhileLocked(base::BindOnce(TestCallback_2));
  }
  std::move(cb2).Run(123, std::string("yo"));
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::OnceCallback<void(int, const std::string&, Param)> cb3;
  {
    ProxyAutoLock lock;
    cb3 = RunWhileLocked(base::BindOnce(TestCallback_3));
  }
  std::move(cb3).Run(123, std::string("yo"), Param());
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::OnceCallback<void(const std::string&)> cb1_string;
  {
    ProxyAutoLock lock;
    cb1_string = RunWhileLocked(base::BindOnce(TestCallback_2, 123));
  }
  std::move(cb1_string).Run(std::string("yo"));
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    cb0 =
        RunWhileLocked(base::BindOnce(TestCallback_2, 123, std::string("yo")));
  }
  std::move(cb0).Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;
}

TEST_F(PpapiProxyLockTest, Unlocking) {
  TestGlobals globals;
  expect_to_be_locked = false;
  // These calls should all try to _unlock_, so we must be locked before
  // entering them.
  ProxyAutoLock auto_lock;

  {
    CallWhileUnlocked(TestCallback_0);
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
  {
    CallWhileUnlocked(TestCallback_1, 123);
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
  {
    CallWhileUnlocked(TestCallback_2, 123, std::string("yo"));
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
  {
    base::OnceCallback<void()> callback(base::BindOnce(TestCallback_0));
    CallWhileUnlocked(std::move(callback));
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
}

}  // namespace ppapi
