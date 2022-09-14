// Copyright 2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/djb2.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

uint8_t kTestData[] = {1, 2, 3};

TEST(DJB2HashTest, HashTest) {
  EXPECT_EQ(DJB2Hash(NULL, 0, 0u), 0u);
  EXPECT_EQ(DJB2Hash(kTestData, sizeof(kTestData), 5381u),
                     ((5381u * 33u + 1u) * 33u + 2u) * 33u + 3u);
}
