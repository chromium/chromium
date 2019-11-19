// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
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
  EXPECT_NE(base::nullopt, response.Date());
  EXPECT_NE(base::nullopt, response.Expires());
  EXPECT_NE(base::nullopt, response.LastModified());
  EXPECT_EQ(true, response.CacheControlContainsNoCache());
}

void RunInThread() {
  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
}

}  // namespace

TEST(ResourceResponseTest, SignedCertificateTimestampIsolatedCopy) {
  ResourceResponse::SignedCertificateTimestamp src(
      "status", "origin", "logDescription", "logId", 7, "hashAlgorithm",
      "signatureAlgorithm", "signatureData");

  ResourceResponse::SignedCertificateTimestamp dest = src.IsolatedCopy();

  EXPECT_EQ(src.status_, dest.status_);
  EXPECT_NE(src.status_.Impl(), dest.status_.Impl());
  EXPECT_EQ(src.origin_, dest.origin_);
  EXPECT_NE(src.origin_.Impl(), dest.origin_.Impl());
  EXPECT_EQ(src.log_description_, dest.log_description_);
  EXPECT_NE(src.log_description_.Impl(), dest.log_description_.Impl());
  EXPECT_EQ(src.log_id_, dest.log_id_);
  EXPECT_NE(src.log_id_.Impl(), dest.log_id_.Impl());
  EXPECT_EQ(src.timestamp_, dest.timestamp_);
  EXPECT_EQ(src.hash_algorithm_, dest.hash_algorithm_);
  EXPECT_NE(src.hash_algorithm_.Impl(), dest.hash_algorithm_.Impl());
  EXPECT_EQ(src.signature_algorithm_, dest.signature_algorithm_);
  EXPECT_NE(src.signature_algorithm_.Impl(), dest.signature_algorithm_.Impl());
  EXPECT_EQ(src.signature_data_, dest.signature_data_);
  EXPECT_NE(src.signature_data_.Impl(), dest.signature_data_.Impl());
}

// This test checks that AtomicStrings in ResourceResponse doesn't cause the
// failure of ThreadRestrictionVerifier check.
TEST(ResourceResponseTest, CrossThreadAtomicStrings) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
  std::unique_ptr<Thread> thread = Platform::Current()->CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
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

}  // namespace blink
