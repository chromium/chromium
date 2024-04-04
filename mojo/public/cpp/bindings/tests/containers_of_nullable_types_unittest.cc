// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/containers_of_nullable_types_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/containers_of_nullable_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::containers_of_nullable_types {
namespace {

class ContainersOfNullables : public ::testing::Test {
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(ContainersOfNullables, ContainersOfOptionals) {
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->i32_values.push_back(std::nullopt);
    input->i32_values.push_back(3);
    input->i32_values.push_back(2);
    input->i32_values.push_back(std::nullopt);

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(output->i32_values, std::vector<std::optional<int32_t>>(
                                      {std::nullopt, 3, 2, std::nullopt}));
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->u64_values.push_back(std::nullopt);
    input->u64_values.push_back(6);
    input->u64_values.push_back(4);
    input->u64_values.push_back(std::nullopt);

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(output->u64_values, std::vector<std::optional<uint64_t>>(
                                      {std::nullopt, 6, 4, std::nullopt}));
  }
  {
    // Test a sufficiently large number of elements to exercise multi-byte
    // bitfield condition.
    auto input = mojom::StructWithContainersOfOptionals::New();
    constexpr uint32_t kNumElements = 1000;
    for (uint32_t i = 0; i < kNumElements; ++i) {
      if (i & 0x1) {
        input->i32_values.push_back(std::nullopt);
      } else {
        input->i32_values.push_back(i);
      }
    }

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    for (uint32_t i = 0; i < kNumElements; ++i) {
      if (i & 0x1) {
        EXPECT_EQ(std::nullopt, output->i32_values[i]);
      } else {
        EXPECT_EQ(i, output->i32_values[i]);
      }
    }
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->enum_values.push_back(mojom::RegularEnum::kThisValue);
    input->enum_values.push_back(std::nullopt);
    input->enum_values.push_back(mojom::RegularEnum::kThatValue);
    input->enum_values.push_back(mojom::RegularEnum::kZeroValue);
    // Force an unknown value to test default behaviour.
    input->enum_values.push_back(static_cast<mojom::RegularEnum>(-10));

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(
        output->enum_values,
        std::vector<std::optional<mojom::RegularEnum>>(
            {mojom::RegularEnum::kThisValue, std::nullopt,
             mojom::RegularEnum::kThatValue, mojom::RegularEnum::kZeroValue,
             mojom::RegularEnum::kUnknown}));
  }
  {
    // Test 9 bools which should require the bitfield to require an additional
    // byte. Check to make sure that the value bytes are properly offset.
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->bool_values.push_back(true);
    input->bool_values.push_back(std::nullopt);
    input->bool_values.push_back(false);
    input->bool_values.push_back(false);
    input->bool_values.push_back(true);
    input->bool_values.push_back(true);
    input->bool_values.push_back(false);
    input->bool_values.push_back(false);
    input->bool_values.push_back(true);

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(output->bool_values, std::vector<std::optional<bool>>(
                                       {true, std::nullopt, false, false, true,
                                        true, false, false, true}));
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->i32_map[3] = 2;
    input->i32_map[44] = std::nullopt;

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(2, output->i32_map[3]);
    EXPECT_EQ(std::nullopt, output->i32_map[44]);
    EXPECT_FALSE(output->i32_map.contains(111));
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->bool_map[10] = true;
    input->bool_map[20] = std::nullopt;

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(true, output->bool_map[10]);
    EXPECT_EQ(std::nullopt, output->bool_map[20]);
    EXPECT_FALSE(output->bool_map.contains(111));
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->enum_map[10] = mojom::RegularEnum::kThisValue;
    input->enum_map[20] = std::nullopt;

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
        input, output));

    EXPECT_EQ(mojom::RegularEnum::kThisValue, output->enum_map[10]);
    EXPECT_EQ(std::nullopt, output->enum_map[20]);
    EXPECT_FALSE(output->enum_map.contains(111));
  }
  {
    auto input = mojom::StructWithContainersOfOptionals::New();

    input->non_extensible_enum_values.push_back(
        static_cast<mojom::NonExtensibleEnum>(-666));

    mojom::StructWithContainersOfOptionalsPtr output;
    ASSERT_FALSE(
        SerializeAndDeserialize<mojom::StructWithContainersOfOptionals>(
            input, output));
  }
  {
    auto input = mojom::TypemappedContainer::New();
    input->enum_values.push_back(mojom::RegularEnum::kThisValue);
    input->enum_values.push_back(std::nullopt);
    // Put a bad value here to ensure that typemapped structs can properly
    // fallback to default.
    input->enum_values.push_back(static_cast<mojom::RegularEnum>(666));

    NativeStruct output;
    ASSERT_TRUE(
        SerializeAndDeserialize<mojom::TypemappedContainer>(input, output));

    EXPECT_EQ(output.enum_values,
              std::vector<std::optional<mojom::RegularEnum>>({
                  mojom::RegularEnum::kThisValue,
                  std::nullopt,
                  mojom::RegularEnum::kUnknown,
              }));
  }
}

}  // namespace
}  // namespace mojo::test::containers_of_nullable_types
