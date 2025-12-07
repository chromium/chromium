// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_context.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using Handle_Data = mojo::internal::Handle_Data;
using AssociatedEndpointHandle_Data =
    mojo::internal::AssociatedEndpointHandle_Data;

const void* ToPtr(uintptr_t ptr) {
  return reinterpret_cast<const void*>(ptr);
}

#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
TEST(ValidationContextTest, ConstructorRangeOverflow) {
  {
    // Test memory range overflow.
    internal::ValidationContext context(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 3000), 5000, 0, 0);

    EXPECT_FALSE(context.IsValidRange(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 3000), 1));
    EXPECT_FALSE(context.ClaimMemory(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 3000), 1));
  }

  if (sizeof(size_t) <= sizeof(uint32_t))
    return;

  {
    // Test handle index range overflow.
    size_t num_handles =
        static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 5;
    internal::ValidationContext context(ToPtr(0), 0, num_handles, 0);

    EXPECT_FALSE(context.ClaimHandle(Handle_Data(0)));
    EXPECT_FALSE(context.ClaimHandle(
        Handle_Data(std::numeric_limits<uint32_t>::max() - 1)));

    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(internal::kEncodedInvalidHandleValue)));
  }

  {
    size_t num_associated_endpoint_handles =
        static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 5;
    internal::ValidationContext context(ToPtr(0), 0, 0,
                                        num_associated_endpoint_handles);

    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(0)));
    EXPECT_FALSE(
        context.ClaimAssociatedEndpointHandle(AssociatedEndpointHandle_Data(
            std::numeric_limits<uint32_t>::max() - 1)));

    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(internal::kEncodedInvalidHandleValue)));
  }
}
#endif

TEST(ValidationContextTest, IsValidRange) {
  {
    internal::ValidationContext context(ToPtr(1234), 100, 0, 0);

    // Basics.
    EXPECT_FALSE(context.IsValidRange(ToPtr(100), 5));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1230), 50));
    EXPECT_TRUE(context.IsValidRange(ToPtr(1234), 5));
    EXPECT_TRUE(context.IsValidRange(ToPtr(1240), 50));
    EXPECT_TRUE(context.IsValidRange(ToPtr(1234), 100));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1234), 101));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1240), 100));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1333), 5));
    EXPECT_FALSE(context.IsValidRange(ToPtr(2234), 5));

    // ClaimMemory() updates the valid range.
    EXPECT_TRUE(context.ClaimMemory(ToPtr(1254), 10));

    EXPECT_FALSE(context.IsValidRange(ToPtr(1234), 1));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1254), 10));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1263), 1));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1263), 10));
    EXPECT_TRUE(context.IsValidRange(ToPtr(1264), 10));
    EXPECT_TRUE(context.IsValidRange(ToPtr(1264), 70));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1264), 71));
  }

  {
    internal::ValidationContext context(ToPtr(1234), 100, 0, 0);
    // Should return false for empty ranges.
    EXPECT_FALSE(context.IsValidRange(ToPtr(0), 0));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1200), 0));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1234), 0));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1240), 0));
    EXPECT_FALSE(context.IsValidRange(ToPtr(2234), 0));
  }

  {
    // The valid memory range is empty.
    internal::ValidationContext context(ToPtr(1234), 0, 0, 0);

    EXPECT_FALSE(context.IsValidRange(ToPtr(1234), 1));
    EXPECT_FALSE(context.IsValidRange(ToPtr(1234), 0));
  }

  {
    internal::ValidationContext context(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 2000), 1000, 0, 0);

    // Test overflow.
    EXPECT_FALSE(context.IsValidRange(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 1500), 4000));
    EXPECT_FALSE(context.IsValidRange(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 1500),
        std::numeric_limits<uint32_t>::max()));

    // This should be fine.
    EXPECT_TRUE(context.IsValidRange(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 1500), 200));
  }
}

TEST(ValidationContextTest, ClaimHandle) {
  {
    internal::ValidationContext context(ToPtr(0), 0, 10, 0);

    // Basics.
    EXPECT_TRUE(context.ClaimHandle(Handle_Data(0)));
    EXPECT_FALSE(context.ClaimHandle(Handle_Data(0)));

    EXPECT_TRUE(context.ClaimHandle(Handle_Data(9)));
    EXPECT_FALSE(context.ClaimHandle(Handle_Data(10)));

    // Should fail because it is smaller than the max index that has been
    // claimed.
    EXPECT_FALSE(context.ClaimHandle(Handle_Data(8)));

    // Should return true for invalid handle.
    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(internal::kEncodedInvalidHandleValue)));
    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(internal::kEncodedInvalidHandleValue)));
  }

  {
    // No handle to claim.
    internal::ValidationContext context(ToPtr(0), 0, 0, 0);

    EXPECT_FALSE(context.ClaimHandle(Handle_Data(0)));

    // Should still return true for invalid handle.
    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(internal::kEncodedInvalidHandleValue)));
  }

  {
    // Test the case that |num_handles| is the same value as
    // |internal::kEncodedInvalidHandleValue|.
    EXPECT_EQ(internal::kEncodedInvalidHandleValue,
              std::numeric_limits<uint32_t>::max());
    internal::ValidationContext context(
        ToPtr(0), 0, std::numeric_limits<uint32_t>::max(), 0);

    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(std::numeric_limits<uint32_t>::max() - 1)));
    EXPECT_FALSE(context.ClaimHandle(
        Handle_Data(std::numeric_limits<uint32_t>::max() - 1)));
    EXPECT_FALSE(context.ClaimHandle(Handle_Data(0)));

    // Should still return true for invalid handle.
    EXPECT_TRUE(context.ClaimHandle(
        Handle_Data(internal::kEncodedInvalidHandleValue)));
  }
}

TEST(ValidationContextTest, ClaimAssociatedEndpointHandle) {
  {
    internal::ValidationContext context(ToPtr(0), 0, 0, 10);

    // Basics.
    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(0)));
    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(0)));

    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(9)));
    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(10)));

    // Should fail because it is smaller than the max index that has been
    // claimed.
    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(8)));

    // Should return true for invalid handle.
    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(internal::kEncodedInvalidHandleValue)));
    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(internal::kEncodedInvalidHandleValue)));
  }

  {
    // No handle to claim.
    internal::ValidationContext context(ToPtr(0), 0, 0, 0);

    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(0)));

    // Should still return true for invalid handle.
    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(internal::kEncodedInvalidHandleValue)));
  }

  {
    // Test the case that |num_associated_endpoint_handles| is the same value as
    // |internal::kEncodedInvalidHandleValue|.
    EXPECT_EQ(internal::kEncodedInvalidHandleValue,
              std::numeric_limits<uint32_t>::max());
    internal::ValidationContext context(ToPtr(0), 0, 0,
                                        std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(
        context.ClaimAssociatedEndpointHandle(AssociatedEndpointHandle_Data(
            std::numeric_limits<uint32_t>::max() - 1)));
    EXPECT_FALSE(
        context.ClaimAssociatedEndpointHandle(AssociatedEndpointHandle_Data(
            std::numeric_limits<uint32_t>::max() - 1)));
    EXPECT_FALSE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(0)));

    // Should still return true for invalid handle.
    EXPECT_TRUE(context.ClaimAssociatedEndpointHandle(
        AssociatedEndpointHandle_Data(internal::kEncodedInvalidHandleValue)));
  }
}

TEST(ValidationContextTest, ClaimMemory) {
  {
    internal::ValidationContext context(ToPtr(1000), 2000, 0, 0);

    // Basics.
    EXPECT_FALSE(context.ClaimMemory(ToPtr(500), 100));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(800), 300));
    EXPECT_TRUE(context.ClaimMemory(ToPtr(1000), 100));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(1099), 100));
    EXPECT_TRUE(context.ClaimMemory(ToPtr(1100), 200));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(2000), 1001));
    EXPECT_TRUE(context.ClaimMemory(ToPtr(2000), 500));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(2000), 500));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(1400), 100));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(3000), 1));
    EXPECT_TRUE(context.ClaimMemory(ToPtr(2500), 500));
  }

  {
    // No memory to claim.
    internal::ValidationContext context(ToPtr(10000), 0, 0, 0);

    EXPECT_FALSE(context.ClaimMemory(ToPtr(10000), 1));
    EXPECT_FALSE(context.ClaimMemory(ToPtr(10000), 0));
  }

  {
    internal::ValidationContext context(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 1000), 500, 0, 0);

    // Test overflow.
    EXPECT_FALSE(context.ClaimMemory(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 750), 4000));
    EXPECT_FALSE(
        context.ClaimMemory(ToPtr(std::numeric_limits<uintptr_t>::max() - 750),
                            std::numeric_limits<uint32_t>::max()));

    // This should be fine.
    EXPECT_TRUE(context.ClaimMemory(
        ToPtr(std::numeric_limits<uintptr_t>::max() - 750), 200));
  }
}

}  // namespace
}  // namespace test
}  // namespace mojo
