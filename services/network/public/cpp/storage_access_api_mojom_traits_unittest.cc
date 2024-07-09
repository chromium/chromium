// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/storage_access_api_mojom_traits.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/storage_access_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(StorageAccessApiTraitsTest, Roundtrips_Status) {
  for (const auto status : {
           net::StorageAccessApiStatus::kNone,
           net::StorageAccessApiStatus::kAccessViaAPI,
       }) {
    net::StorageAccessApiStatus roundtrip;
    ASSERT_TRUE(
        test::SerializeAndDeserialize<network::mojom::StorageAccessApiStatus>(
            status, roundtrip));
    EXPECT_EQ(status, roundtrip);
  }
}

}  // namespace
}  // namespace mojo
