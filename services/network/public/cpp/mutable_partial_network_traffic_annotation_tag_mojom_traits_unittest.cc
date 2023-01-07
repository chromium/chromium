// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/mutable_partial_network_traffic_annotation_tag_mojom_traits.h"

#include "base/check_op.h"
#include "services/network/public/mojom/mutable_partial_network_traffic_annotation_tag.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(MutablePartialNetworkTrafficAnnottionTagsTest, BasicTest) {
  net::MutablePartialNetworkTrafficAnnotationTag original;
  net::MutablePartialNetworkTrafficAnnotationTag copy;

  original.unique_id_hash_code = 1;
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  original.completing_id_hash_code = 2;
#endif
  EXPECT_TRUE(mojom::MutablePartialNetworkTrafficAnnotationTag::Deserialize(
      mojom::MutablePartialNetworkTrafficAnnotationTag::Serialize(&original),
      &copy));
  EXPECT_EQ(copy.unique_id_hash_code, 1);
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_EQ(copy.completing_id_hash_code, 2);
#endif
}

}  // namespace network
