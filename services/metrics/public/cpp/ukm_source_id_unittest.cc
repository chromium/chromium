// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/metrics/public/cpp/ukm_source_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

TEST(UkmSourceIdTest, AssignSourceIds) {
  const size_t numIds = 5;
  SourceId ids[numIds] = {};

  for (size_t i = 0; i < numIds; i++) {
    ids[i] = AssignNewSourceId();
    EXPECT_NE(kInvalidSourceId, ids[i]);
    EXPECT_EQ(SourceIdType::DEFAULT, GetSourceIdType(ids[i]));
    for (size_t j = 0; j < i; j++) {
      EXPECT_NE(ids[j], ids[i]);
    }
  }
}

TEST(UkmSourceIdTest, ConvertToNavigationType) {
  const size_t numIds = 5;
  SourceId ids[numIds] = {};

  for (size_t i = 0; i < numIds; i++) {
    ids[i] = ConvertToSourceId(i, SourceIdType::NAVIGATION_ID);
    EXPECT_NE(kInvalidSourceId, ids[i]);
    EXPECT_EQ(SourceIdType::NAVIGATION_ID, GetSourceIdType(ids[i]));
    for (size_t j = 0; j < i; j++) {
      EXPECT_NE(ids[j], ids[i]);
    }
  }
}

TEST(UkmSourceIdTest, GetSourceIdType) {
  // Check that the newly assigned id has the default type.
  const SourceId new_id = AssignNewSourceId();
  EXPECT_EQ(SourceIdType::DEFAULT, GetSourceIdType(new_id));

  const int64_t num_types = static_cast<int64_t>(SourceIdType::kMaxValue);
  for (int64_t type_index = 0; type_index <= num_types; type_index++) {
    const SourceIdType expected_type = SourceIdType(type_index);
    if (expected_type == SourceIdType::WEBAPK_ID ||
        expected_type == SourceIdType::PAYMENT_APP_ID ||
        expected_type == SourceIdType::WEB_IDENTITY_ID) {
      // See comment in ConvertToSourceId regarding these special cases.
      continue;
    }
    // Convert the new id to each existing type and verify that the type
    // information is correctly set on the converted id.
    const SourceId converted_id = ConvertToSourceId(new_id, expected_type);
    EXPECT_EQ(expected_type, GetSourceIdType(converted_id));
  }
}

}  // namespace ukm
