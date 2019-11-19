// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
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
  void Method() { ++called_num; }

 private:
  friend class base::RefCounted<CheckLockStateInDestructor>;
  ~CheckLockStateInDestructor() { CheckLockState(); }
  DISALLOW_COPY_AND_ASSIGN(CheckLockStateInDestructor);
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

  base::Callback<void()> cb0;
  {
    ProxyAutoLock lock;
    cb0 = RunWhileLocked(base::Bind(TestCallback_0));
  }
  cb0.Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    cb0 = RunWhileLocked(base::Bind(TestCallback_1, 123));
  }
  cb0.Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    scoped_refptr<CheckLockStateInDestructor> object =
        new CheckLockStateInDestructor();
    cb0 =
        RunWhileLocked(base::Bind(&CheckLockStateInDestructor::Method, object));
    // Note after this scope, the Callback owns the only reference.
  }
  cb0.Run();
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::Callback<void(int)> cb1;
  {
    ProxyAutoLock lock;
    cb1 = RunWhileLocked(base::Bind(TestCallback_1));
  }
  cb1.Run(123);
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::Callback<void(int, const std::string&)> cb2;
  {
    ProxyAutoLock lock;
    cb2 = RunWhileLocked(base::Bind(TestCallback_2));
  }
  cb2.Run(123, std::string("yo"));
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::Callback<void(int, const std::string&, Param)> cb3;
  {
    ProxyAutoLock lock;
    cb3 = RunWhileLocked(base::Bind(TestCallback_3));
  }
  cb3.Run(123, std::string("yo"), Param());
  ASSERT_EQ(1, called_num);
  called_num = 0;

  base::Callback<void(const std::string&)> cb1_string;
  {
    ProxyAutoLock lock;
    cb1_string = RunWhileLocked(base::Bind(TestCallback_2, 123));
  }
  cb1_string.Run(std::string("yo"));
  ASSERT_EQ(1, called_num);
  called_num = 0;

  {
    ProxyAutoLock lock;
    cb0 = RunWhileLocked(base::Bind(TestCallback_2, 123, std::string("yo")));
  }
  cb0.Run();
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
    // TODO(dmichael): Make const-ref arguments work properly with type
    // deduction.
    CallWhileUnlocked<void, int, const std::string&>(
        TestCallback_2, 123, std::string("yo"));
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
  {
    base::Callback<void()> callback(base::Bind(TestCallback_0));
    CallWhileUnlocked(callback);
    ASSERT_EQ(1, called_num);
    called_num = 0;
  }
}

}  // namespace ppapi
