// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_resource_timing_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

scoped_refptr<ResourceLoadTiming> CreateResourceLoadTiming(
    const base::TimeTicks pseudo_time) {
  auto timing = ResourceLoadTiming::Create();
  timing->SetRequestTime(pseudo_time);
  timing->SetProxyStart(pseudo_time);
  timing->SetProxyEnd(pseudo_time);
  timing->SetDnsStart(pseudo_time);
  timing->SetDnsEnd(pseudo_time);
  timing->SetConnectStart(pseudo_time);
  timing->SetConnectEnd(pseudo_time);
  timing->SetWorkerStart(pseudo_time);
  timing->SetWorkerReady(pseudo_time);
  timing->SetSendStart(pseudo_time);
  timing->SetSendEnd(pseudo_time);
  timing->SetReceiveHeadersStart(pseudo_time);
  timing->SetReceiveHeadersEnd(pseudo_time);
  timing->SetSslStart(pseudo_time);
  timing->SetSslStart(pseudo_time);
  timing->SetSslEnd(pseudo_time);
  timing->SetPushEnd(pseudo_time);
  return timing;
}

WebResourceTimingInfo CreateWebResourceTimingInfo(
    const base::TimeTicks pseudo_time) {
  WebVector<WebServerTimingInfo> server_timing;
  server_timing.emplace_back(WebServerTimingInfo("server_timing_name", 1.0,
                                                 "server_timing_description"));

  WebResourceTimingInfo info = {
      .name = "name",
      .start_time = pseudo_time,

      .alpn_negotiated_protocol = "protocol",
      .connection_info = "info",

      .timing = CreateResourceLoadTiming(pseudo_time),
      .last_redirect_end_time = pseudo_time,
      .response_end = pseudo_time,

      .transfer_size = 1,
      .encoded_body_size = 2,
      .decoded_body_size = 3,

      .did_reuse_connection = true,
      .is_secure_context = true,

      .allow_timing_details = true,
      .allow_redirect_details = true,

      .allow_negative_values = true,

      .server_timing = server_timing,
  };
  return info;
}

void CheckWebResourceTimingInfoOnThread(const WebResourceTimingInfo& info,
                                        const base::TimeTicks pseudo_time) {
  WebResourceTimingInfo expected = CreateWebResourceTimingInfo(pseudo_time);
  EXPECT_EQ(expected, info);

  EXPECT_EQ("name", info.name);
  EXPECT_EQ(pseudo_time, info.start_time);

  EXPECT_EQ("protocol", info.alpn_negotiated_protocol);
  EXPECT_EQ("info", info.connection_info);

  WebURLLoadTiming expected_timing(CreateResourceLoadTiming(pseudo_time));
  EXPECT_EQ(expected_timing, info.timing);
  EXPECT_EQ(pseudo_time, info.last_redirect_end_time);
  EXPECT_EQ(pseudo_time, info.response_end);

  EXPECT_EQ(1u, info.transfer_size);
  EXPECT_EQ(2u, info.encoded_body_size);
  EXPECT_EQ(3u, info.decoded_body_size);

  EXPECT_TRUE(info.did_reuse_connection);
  EXPECT_TRUE(info.is_secure_context);

  EXPECT_TRUE(info.allow_timing_details);
  EXPECT_TRUE(info.allow_redirect_details);

  EXPECT_TRUE(info.allow_negative_values);

  EXPECT_EQ(1u, info.server_timing.size());

  auto& entry = info.server_timing[0];
  EXPECT_EQ("server_timing_name", entry.name);
  EXPECT_EQ(1.0, entry.duration);
  EXPECT_EQ("server_timing_description", entry.description);
}

}  // namespace

TEST(WebResourceTimingInfoTest, CrossThreadCopy) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  base::TimeTicks pseudo_time = base::TimeTicks::Now();
  WebResourceTimingInfo info = CreateWebResourceTimingInfo(pseudo_time);

  std::unique_ptr<Thread> thread = Platform::Current()->CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
          .SetThreadNameForTest("TestThread"));
  PostCrossThreadTask(*thread->GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&CheckWebResourceTimingInfoOnThread,
                                          info, pseudo_time));
}

}  // namespace blink
