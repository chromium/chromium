// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

ResourceResponse CreateTestResponse() {
  ResourceResponse response;
  response.AddHttpHeaderField(http_names::kLowerAge, AtomicString("0"));
  response.AddHttpHeaderField(http_names::kCacheControl,
                              AtomicString("no-cache"));
  response.AddHttpHeaderField(http_names::kDate,
                              AtomicString("Tue, 17 Jan 2017 04:01:00 GMT"));
  response.AddHttpHeaderField(http_names::kExpires,
                              AtomicString("Tue, 17 Jan 2017 04:11:00 GMT"));
  response.AddHttpHeaderField(http_names::kLastModified,
                              AtomicString("Tue, 17 Jan 2017 04:00:00 GMT"));
  response.AddHttpHeaderField(http_names::kPragma, AtomicString("public"));
  response.AddHttpHeaderField(http_names::kETag, AtomicString("abc"));
  response.AddHttpHeaderField(http_names::kContentDisposition,
                              AtomicString("attachment; filename=a.txt"));
  return response;
}

}  // namespace

TEST(ResourceResponseTest, AddHttpHeaderFieldWithMultipleValues) {
  ResourceResponse response(CreateTestResponse());

  Vector<AtomicString> empty_values;
  response.AddHttpHeaderFieldWithMultipleValues(http_names::kLowerSetCookie,
                                                empty_values);
  EXPECT_EQ(AtomicString(),
            response.HttpHeaderField(http_names::kLowerSetCookie));

  response.AddHttpHeaderField(http_names::kLowerSetCookie, AtomicString("a=1"));
  EXPECT_EQ("a=1", response.HttpHeaderField(http_names::kLowerSetCookie));

  Vector<AtomicString> values;
  values.push_back("b=2");
  values.push_back("c=3");
  response.AddHttpHeaderFieldWithMultipleValues(http_names::kLowerSetCookie,
                                                values);

  EXPECT_EQ("a=1, b=2, c=3",
            response.HttpHeaderField(http_names::kLowerSetCookie));
}

TEST(ResourceResponseTest, DnsAliasesCanBeSetAndAccessed) {
  ResourceResponse response(CreateTestResponse());

  EXPECT_TRUE(response.DnsAliases().empty());

  Vector<String> aliases({"alias1", "alias2"});
  response.SetDnsAliases(aliases);

  EXPECT_THAT(response.DnsAliases(), testing::ElementsAre("alias1", "alias2"));
}

TEST(ResourceResponseTest, TreatExpiresZeroAsExpired) {
  ResourceResponse response(CreateTestResponse());

  response.SetHttpHeaderField(http_names::kExpires, AtomicString("0"));

  std::optional<base::Time> expires = response.Expires();
  EXPECT_EQ(base::Time::Min(), expires);

  base::Time creation_time = base::Time::UnixEpoch();
  base::TimeDelta calculated_expires = expires.value() - creation_time;
  // Check the value is not overflow by ClampedNumeric after subtracting value
  EXPECT_EQ(base::TimeDelta::Min(), calculated_expires);
}

}  // namespace blink
