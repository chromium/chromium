// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

ResourceResponse CreateTestResponse() {
  ResourceResponse response;
  response.AddHttpHeaderField("age", "0");
  response.AddHttpHeaderField("cache-control", "no-cache");
  response.AddHttpHeaderField("date", "Tue, 17 Jan 2017 04:01:00 GMT");
  response.AddHttpHeaderField("expires", "Tue, 17 Jan 2017 04:11:00 GMT");
  response.AddHttpHeaderField("last-modified", "Tue, 17 Jan 2017 04:00:00 GMT");
  response.AddHttpHeaderField("pragma", "public");
  response.AddHttpHeaderField("etag", "abc");
  response.AddHttpHeaderField("content-disposition",
                              "attachment; filename=a.txt");
  return response;
}

void RunHeaderRelatedTest(const ResourceResponse& response) {
  EXPECT_EQ(base::TimeDelta(), response.Age());
  EXPECT_NE(absl::nullopt, response.Date());
  EXPECT_NE(absl::nullopt, response.Expires());
  EXPECT_NE(absl::nullopt, response.LastModified());
  EXPECT_EQ(true, response.CacheControlContainsNoCache());
}

void RunInThread() {
  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
}

}  // namespace

// This test checks that AtomicStrings in ResourceResponse doesn't cause the
// failure of ThreadRestrictionVerifier check.
TEST(ResourceResponseTest, CrossThreadAtomicStrings) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
  std::unique_ptr<NonMainThread> thread =
      NonMainThread::CreateThread(ThreadCreationParams(ThreadType::kTestThread)
                                      .SetThreadNameForTest("WorkerThread"));
  PostCrossThreadTask(*thread->GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&RunInThread));
  thread.reset();
}

TEST(ResourceResponseTest, AddHttpHeaderFieldWithMultipleValues) {
  ResourceResponse response(CreateTestResponse());

  Vector<AtomicString> empty_values;
  response.AddHttpHeaderFieldWithMultipleValues("set-cookie", empty_values);
  EXPECT_EQ(AtomicString(), response.HttpHeaderField("set-cookie"));

  response.AddHttpHeaderField("set-cookie", "a=1");
  EXPECT_EQ("a=1", response.HttpHeaderField("set-cookie"));

  Vector<AtomicString> values;
  values.push_back("b=2");
  values.push_back("c=3");
  response.AddHttpHeaderFieldWithMultipleValues("set-cookie", values);

  EXPECT_EQ("a=1, b=2, c=3", response.HttpHeaderField("set-cookie"));
}

TEST(ResourceResponseTest, DnsAliasesCanBeSetAndAccessed) {
  ResourceResponse response(CreateTestResponse());

  EXPECT_TRUE(response.DnsAliases().IsEmpty());

  Vector<String> aliases({"alias1", "alias2"});
  response.SetDnsAliases(aliases);

  EXPECT_THAT(response.DnsAliases(), testing::ElementsAre("alias1", "alias2"));
}

}  // namespace blink
