// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/source_location.h"

#include <optional>
#include <string_view>

#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// The name of the GetTestLocation function.
static constexpr std::string_view kTestFunctionName = "GetTestLocation";

// The name of this file.
static constexpr std::string_view kTestFileName = __FILE_NAME__;

SourceLocation GetTestLocation() {
  return FROM_HERE;
}

SourceLocation GetTestLocationWithBackingStore() {
  SourceLocation location_without_backing_store = GetTestLocation();
  return SourceLocation::CreateWithBackingStoreForTesting(
      location_without_backing_store.function_name(),
      location_without_backing_store.file_name(),
      location_without_backing_store.line_number());
}

}  // namespace

TEST(SourceLocation, Equality) {
  SourceLocation null_location_1;
  SourceLocation null_location_2;
  SourceLocation non_null_location_1 = FROM_HERE;
  SourceLocation non_null_location_2 = FROM_HERE;

  ASSERT_EQ(null_location_1, null_location_1);
  ASSERT_EQ(null_location_1, null_location_2);
  ASSERT_EQ(non_null_location_1, non_null_location_1);
  ASSERT_NE(null_location_1, non_null_location_1);
  ASSERT_NE(non_null_location_1, non_null_location_2);
}

TEST(SourceLocation, CreateWithBaseLocation) {
  SourceLocation location = GetTestLocation();
  ASSERT_FALSE(location.is_null());
  ASSERT_FALSE(location.HasBackingStoreForTesting());
  ASSERT_EQ(std::string_view(location.function_name()), kTestFunctionName);
  ASSERT_TRUE(std::string_view(location.file_name()).ends_with(kTestFileName));
  ASSERT_GT(location.line_number(), -1);

  std::string location_string = location.ToString();
  ASSERT_NE(location_string.find(kTestFunctionName), std::string::npos);
  ASSERT_NE(location_string.find(kTestFileName), std::string::npos);
}

TEST(SourceLocation, CreateWithBackingStore) {
  SourceLocation location = GetTestLocationWithBackingStore();
  ASSERT_FALSE(location.is_null());
  ASSERT_TRUE(location.HasBackingStoreForTesting());
  ASSERT_EQ(std::string_view(location.function_name()), kTestFunctionName);
  ASSERT_TRUE(std::string_view(location.file_name()).ends_with(kTestFileName));
  ASSERT_GT(location.line_number(), -1);

  std::string location_string = location.ToString();
  ASSERT_NE(location_string.find(kTestFunctionName), std::string::npos);
  ASSERT_NE(location_string.find(kTestFileName), std::string::npos);
}

TEST(SourceLocation, CreateWithNullBaseLocation) {
  SourceLocation location = base::Location();
  ASSERT_TRUE(location.is_null());
  ASSERT_FALSE(location.HasBackingStoreForTesting());
  ASSERT_EQ(location.function_name(), nullptr);
  ASSERT_EQ(location.file_name(), nullptr);
  ASSERT_EQ(location.line_number(), -1);
}

TEST(SourceLocation,
     CreateNullLocationWithBackingStore_BackingStoreNotAllocated) {
  SourceLocation location = SourceLocation::CreateWithBackingStoreForTesting(
      std::nullopt, std::nullopt, -1);
  ASSERT_TRUE(location.is_null());
  ASSERT_FALSE(location.HasBackingStoreForTesting());
  ASSERT_EQ(location.function_name(), nullptr);
  ASSERT_EQ(location.file_name(), nullptr);
  ASSERT_EQ(location.line_number(), -1);
}

TEST(SourceLocation, MoveWithoutBackingStore) {
  SourceLocation location_1 = GetTestLocation();
  ASSERT_FALSE(location_1.HasBackingStoreForTesting());
  const char* function_name = location_1.function_name();
  const char* file_name = location_1.file_name();
  int line_number = location_1.line_number();
  SourceLocation location_2 = std::move(location_1);

  ASSERT_FALSE(location_2.HasBackingStoreForTesting());
  ASSERT_EQ(location_2.function_name(), function_name);
  ASSERT_EQ(location_2.file_name(), file_name);
  ASSERT_EQ(location_2.line_number(), line_number);
}

TEST(SourceLocation, MoveWithBackingStore) {
  SourceLocation location_1 = GetTestLocationWithBackingStore();
  ASSERT_TRUE(location_1.HasBackingStoreForTesting());
  const char* function_name = location_1.function_name();
  const char* file_name = location_1.file_name();
  int line_number = location_1.line_number();
  SourceLocation location_2 = std::move(location_1);

  ASSERT_TRUE(location_2.HasBackingStoreForTesting());
  ASSERT_EQ(location_2.function_name(), function_name);
  // The backing store ownership has been transferred, while its memory remains
  // unchanged.
  ASSERT_EQ(location_2.file_name(), file_name);
  ASSERT_EQ(location_2.line_number(), line_number);
}

TEST(SourceLocation, CopyWithoutBackingStore) {
  SourceLocation location_1 = GetTestLocation();
  ASSERT_FALSE(location_1.HasBackingStoreForTesting());
  SourceLocation location_2 = location_1;

  ASSERT_FALSE(location_2.HasBackingStoreForTesting());
  // They point to the same addresses.
  ASSERT_EQ(location_2.function_name(), location_1.function_name());
  ASSERT_EQ(location_2.file_name(), location_1.file_name());
  ASSERT_EQ(location_2.line_number(), location_1.line_number());
}

TEST(SourceLocation, CopyWithBackingStore) {
  SourceLocation location_1 = GetTestLocationWithBackingStore();
  ASSERT_TRUE(location_1.HasBackingStoreForTesting());
  SourceLocation location_2 = location_1;

  ASSERT_TRUE(location_2.HasBackingStoreForTesting());
  // The addresses are different but the strings are the same.
  ASSERT_NE(location_2.function_name(), location_1.function_name());
  ASSERT_NE(location_2.file_name(), location_1.file_name());
  ASSERT_EQ(std::string_view(location_2.function_name()),
            std::string_view(location_1.function_name()));
  ASSERT_EQ(std::string_view(location_2.file_name()),
            std::string_view(location_1.file_name()));
  ASSERT_EQ(location_2.line_number(), location_1.line_number());
}

}  // namespace remoting
