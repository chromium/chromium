// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/platform_dns_query_executor_android.h"

#include <android/multinetwork.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class PlatformDnsQueryExecutorAndroidTest : public TestWithTaskEnvironment {};

TEST_F(PlatformDnsQueryExecutorAndroidTest, FailOnNonExistentDomain) {
  if (__builtin_available(android 29, *)) {
    PlatformDnsQueryExecutorAndroid executor(
        "www.this-domain-definitely-does-not-exists-123abc.com",
        handles::kInvalidNetworkHandle);

    AddressList addr_list;
    int os_error = -1;
    int net_error = -1;

    base::RunLoop run_loop;

    PlatformDnsQueryExecutorAndroid::ResultCallback callback = base::BindOnce(
        [](base::OnceClosure quit_closure, AddressList* out_addr_list,
           int* out_os_error, int* out_net_error, const AddressList& addresses,
           int os_error, int net_error) {
          *out_addr_list = addresses;
          *out_os_error = os_error;
          *out_net_error = net_error;

          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &addr_list, &os_error, &net_error);

    executor.Start(std::move(callback));

    run_loop.Run();

    EXPECT_TRUE(addr_list.empty());
    // TODO(https://crbug.com/451982546): Mock `android_res_nquery/result` to
    // control the return values, and then re-enable this check.
    // EXPECT_EQ(os_error, 0);
    EXPECT_NE(net_error, OK);
  } else {
    GTEST_SKIP_(
        "This test is skipped because it's being run on Android 28-, while the "
        "class that it tests is available only on Android 29+.");
  }
}

}  // namespace
}  // namespace net
