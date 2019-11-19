// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {
namespace {

IPEndPoint MakeEndPoint(const std::string& str) {
  IPAddress addr;
  CHECK(addr.AssignFromIPLiteral(str));
  return IPEndPoint(addr, 0);
}

void OnSortComplete(AddressList* result_buf,
                    CompletionOnceCallback callback,
                    bool success,
                    const AddressList& result) {
  if (success)
    *result_buf = result;
  std::move(callback).Run(success ? OK : ERR_FAILED);
}

TEST(AddressSorterTest, Sort) {
  int expected_result = OK;
#if defined(OS_WIN)
  base::test::TaskEnvironment task_environment;
  EnsureWinsockInit();
  SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    expected_result = ERR_FAILED;
  } else {
    closesocket(sock);
  }
#endif
  std::unique_ptr<AddressSorter> sorter(AddressSorter::CreateAddressSorter());
  AddressList list;
  list.push_back(MakeEndPoint("10.0.0.1"));
  list.push_back(MakeEndPoint("8.8.8.8"));
  list.push_back(MakeEndPoint("::1"));
  list.push_back(MakeEndPoint("2001:4860:4860::8888"));

  AddressList result;
  TestCompletionCallback callback;
  sorter->Sort(list,
               base::BindOnce(&OnSortComplete, &result, callback.callback()));
  EXPECT_EQ(expected_result, callback.WaitForResult());
}

}  // namespace
}  // namespace net
