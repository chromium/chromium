// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using ::testing::ElementsAreArray;

namespace blink {
namespace {

TEST(FetchHeaderListTest, Append) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  headerList->Append("ConTenT-TyPe", "text/plain");
  headerList->Append("content-type", "application/xml");
  headerList->Append("CONTENT-type", "foo");
  headerList->Append("X-Foo", "bar");
  const auto kExpectedHeaders = std::to_array<std::pair<String, String>>({
      {"ConTenT-TyPe", "text/plain"},
      {"ConTenT-TyPe", "application/xml"},
      {"ConTenT-TyPe", "foo"},
      {"X-Foo", "bar"},
  });
  EXPECT_THAT(headerList->List(), ElementsAreArray(kExpectedHeaders));
}

TEST(FetchHeaderListTest, Set) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  headerList->Append("ConTenT-TyPe", "text/plain");
  headerList->Append("content-type", "application/xml");
  headerList->Append("CONTENT-type", "foo");
  headerList->Append("X-Foo", "bar");
  headerList->Set("contENT-type", "quux");
  headerList->Set("some-header", "some value");
  EXPECT_EQ(3U, headerList->size());
  const auto kExpectedHeaders = std::to_array<std::pair<String, String>>({
      {"ConTenT-TyPe", "quux"},
      {"some-header", "some value"},
      {"X-Foo", "bar"},
  });
  EXPECT_THAT(headerList->List(), ElementsAreArray(kExpectedHeaders));
}

TEST(FetchHeaderListTest, Erase) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  headerList->Remove("foo");
  EXPECT_EQ(0U, headerList->size());
  headerList->Append("ConTenT-TyPe", "text/plain");
  headerList->Append("content-type", "application/xml");
  headerList->Append("CONTENT-type", "foo");
  headerList->Append("X-Foo", "bar");
  headerList->Remove("content-TYPE");
  EXPECT_EQ(1U, headerList->size());
  const auto kExpectedHeaders = std::to_array<std::pair<String, String>>({
      {"X-Foo", "bar"},
  });
  EXPECT_THAT(headerList->List(), ElementsAreArray(kExpectedHeaders));
}

TEST(FetchHeaderListTest, Combine) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  headerList->Append("ConTenT-TyPe", "text/plain");
  headerList->Append("content-type", "application/xml");
  headerList->Append("CONTENT-type", "foo");
  headerList->Append("X-Foo", "bar");
  String combinedValue;
  EXPECT_TRUE(headerList->Get("X-Foo", combinedValue));
  EXPECT_EQ("bar", combinedValue);
  EXPECT_TRUE(headerList->Get("content-TYPE", combinedValue));
  EXPECT_EQ("text/plain, application/xml, foo", combinedValue);
}

TEST(FetchHeaderListTest, SetCookie) {
  test::TaskEnvironment task_environment;
  const String values[] = {"foo=bar", "bar=baz; Domain=example.com",
                           "fizz=buzz; Expires=Thu, 01 Jan 1970 00:00:00 GMT"};

  auto* header_list = MakeGarbageCollected<FetchHeaderList>();
  header_list->Append("Set-cookie", values[0]);
  header_list->Append("set-cookie", values[1]);
  header_list->Append("sEt-cOoKiE", values[2]);

  String combined_value;
  EXPECT_TRUE(header_list->Get("Set-Cookie", combined_value));
  EXPECT_EQ(
      "foo=bar, bar=baz; Domain=example.com, "
      "fizz=buzz; Expires=Thu, 01 Jan 1970 00:00:00 GMT",
      combined_value);
  EXPECT_THAT(header_list->GetSetCookie(), ElementsAreArray(values));
}

TEST(FetchHeaderListTest, Contains) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  headerList->Append("ConTenT-TyPe", "text/plain");
  headerList->Append("content-type", "application/xml");
  headerList->Append("X-Foo", "bar");
  EXPECT_TRUE(headerList->Has("CONTENT-TYPE"));
  EXPECT_TRUE(headerList->Has("X-Foo"));
  EXPECT_FALSE(headerList->Has("X-Bar"));
}

TEST(FetchHeaderListTest, SortAndCombine) {
  test::TaskEnvironment task_environment;
  auto* headerList = MakeGarbageCollected<FetchHeaderList>();
  EXPECT_TRUE(headerList->SortAndCombine().empty());
  headerList->Append("Set-cookie", "foo=bar");
  headerList->Append("content-type", "multipart/form-data");
  headerList->Append("ConTenT-TyPe", "application/xml");
  headerList->Append("Accept", "XYZ");
  headerList->Append("X-Foo", "bar");
  headerList->Append("sEt-CoOkIe", "bar=foo");
  const auto kExpectedHeaders = std::to_array<std::pair<String, String>>({
      {"accept", "XYZ"},
      {"content-type", "multipart/form-data, application/xml"},
      {"set-cookie", "foo=bar"},
      {"set-cookie", "bar=foo"},
      {"x-foo", "bar"},
  });
  EXPECT_THAT(headerList->SortAndCombine(), ElementsAreArray(kExpectedHeaders));
}

}  // namespace
}  // namespace blink
