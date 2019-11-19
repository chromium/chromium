// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include "base/strings/pattern.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class URLRequestContextMemoryDumpTest
    : public testing::TestWithParam<base::trace_event::MemoryDumpLevelOfDetail>,
      public WithTaskEnvironment {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    URLRequestContextMemoryDumpTest,
    ::testing::Values(base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
                      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND));

// Checks if the dump provider runs without crashing and dumps root objects.
TEST_P(URLRequestContextMemoryDumpTest, MemoryDumpProvider) {
  base::trace_event::MemoryDumpArgs dump_args = {GetParam()};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(dump_args));
  URLRequestContextBuilder builder;
#if defined(OS_LINUX) || defined(OS_ANDROID)
  builder.set_proxy_config_service(std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation::CreateDirect()));
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
  std::unique_ptr<URLRequestContext> context(builder.Build());
  context->OnMemoryDump(dump_args, process_memory_dump.get());
  const base::trace_event::ProcessMemoryDump::AllocatorDumpsMap&
      allocator_dumps = process_memory_dump->allocator_dumps();

  bool did_dump_http_network_session = false;
  bool did_dump_ssl_client_session_cache = false;
  bool did_dump_url_request_context = false;
  bool did_dump_url_request_context_http_network_session = false;
  for (const auto& it : allocator_dumps) {
    const std::string& dump_name = it.first;
    if (dump_name.find("net/http_network_session") != std::string::npos)
      did_dump_http_network_session = true;
    // Match against a relaxed form of the memory dump permitted pattern.
    if (base::MatchPattern(
            dump_name, "net/http_network_session_0x*/ssl_client_session_cache"))
      did_dump_ssl_client_session_cache = true;
    if (dump_name.find("net/url_request_context") != std::string::npos) {
      // A sub allocator dump to take into account of the sharing relationship.
      if (dump_name.find("http_network_session") != std::string::npos) {
        did_dump_url_request_context_http_network_session = true;
      } else {
        did_dump_url_request_context = true;
      }
    }
  }
  ASSERT_TRUE(did_dump_http_network_session);
  ASSERT_TRUE(did_dump_ssl_client_session_cache);
  ASSERT_TRUE(did_dump_url_request_context);
  ASSERT_TRUE(did_dump_url_request_context_http_network_session);
}

// TODO(xunjieli): Add more granular tests on the MemoryDumpProvider.
}  // namespace net
