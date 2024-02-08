// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/network_delegate_error_observer.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestNetworkDelegate : public NetworkDelegateImpl {
 public:
  TestNetworkDelegate() = default;
  ~TestNetworkDelegate() override = default;

  bool got_pac_error() const { return got_pac_error_; }

 private:
  // NetworkDelegate implementation.
  void OnPACScriptError(int line_number, const std::u16string& error) override {
    got_pac_error_ = true;
  }

  bool got_pac_error_ = false;
};

// Check that the OnPACScriptError method can be called from an arbitrary
// thread.
TEST(NetworkDelegateErrorObserverTest, CallOnThread) {
  base::test::TaskEnvironment task_environment;
  base::Thread thread("test_thread");
  thread.Start();
  TestNetworkDelegate network_delegate;
  NetworkDelegateErrorObserver observer(
      &network_delegate,
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkDelegateErrorObserver::OnPACScriptError,
                     base::Unretained(&observer), 42, std::u16string()));
  thread.Stop();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(network_delegate.got_pac_error());
}

// Check that passing a NULL network delegate works.
TEST(NetworkDelegateErrorObserverTest, NoDelegate) {
  base::test::TaskEnvironment task_environment;
  base::Thread thread("test_thread");
  thread.Start();
  NetworkDelegateErrorObserver observer(
      nullptr, base::SingleThreadTaskRunner::GetCurrentDefault().get());
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkDelegateErrorObserver::OnPACScriptError,
                     base::Unretained(&observer), 42, std::u16string()));
  thread.Stop();
  base::RunLoop().RunUntilIdle();
  // Shouldn't have crashed until here...
}

}  // namespace

}  // namespace net
