// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#endif

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {
namespace {

IPEndPoint MakeEndPoint(const std::string& str) {
  IPAddress addr;
  CHECK(addr.AssignFromIPLiteral(str));
  return IPEndPoint(addr, 0);
}

void OnSortComplete(std::vector<IPEndPoint>* sorted_buf,
                    CompletionOnceCallback callback,
                    bool success,
                    std::vector<IPEndPoint> sorted) {
  if (success)
    *sorted_buf = std::move(sorted);
  std::move(callback).Run(success ? OK : ERR_FAILED);
}

TEST(AddressSorterTest, Sort) {
  base::test::TaskEnvironment task_environment;
  int expected_result = OK;
#if BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
  SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    expected_result = ERR_FAILED;
  } else {
    closesocket(sock);
  }
#endif
  std::unique_ptr<AddressSorter> sorter(AddressSorter::CreateAddressSorter());
  std::vector<IPEndPoint> endpoints;
  endpoints.push_back(MakeEndPoint("10.0.0.1"));
  endpoints.push_back(MakeEndPoint("8.8.8.8"));
  endpoints.push_back(MakeEndPoint("::1"));
  endpoints.push_back(MakeEndPoint("2001:4860:4860::8888"));

  std::vector<IPEndPoint> result;
  TestCompletionCallback callback;
  sorter->Sort(endpoints,
               base::BindOnce(&OnSortComplete, &result, callback.callback()));
  EXPECT_EQ(expected_result, callback.WaitForResult());
}

}  // namespace
}  // namespace net
