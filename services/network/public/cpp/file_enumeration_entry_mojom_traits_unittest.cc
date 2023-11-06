// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/file_enumeration_entry_mojom_traits.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(FileEnumerationEntryMojomTraitsTest, SuccessCase) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  constexpr int64_t size = 19;
  constexpr base::Time last_accessed =
      base::Time::UnixEpoch() + base::Seconds(99);
  constexpr base::Time last_modified =
      base::Time::UnixEpoch() + base::Seconds(33);

  const disk_cache::BackendFileOperations::FileEnumerationEntry original(
      path, size, last_accessed, last_modified);

  disk_cache::BackendFileOperations::FileEnumerationEntry deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FileEnumerationEntry>(
      original, deserialized));

  EXPECT_EQ(deserialized.path, original.path);
  EXPECT_EQ(deserialized.size, original.size);
  EXPECT_EQ(deserialized.last_accessed, original.last_accessed);
  EXPECT_EQ(deserialized.last_modified, original.last_modified);
}

}  // namespace
}  // namespace network
