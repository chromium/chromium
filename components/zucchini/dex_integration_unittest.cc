// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/zucchini.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

void TestDexZipRoundTrip(const base::FilePath& src_path,
                         const base::FilePath& dst_path) {
  base::MemoryMappedFile src_file;
  ASSERT_TRUE(src_file.Initialize(src_path));

  base::MemoryMappedFile dst_file;
  ASSERT_TRUE(dst_file.Initialize(dst_path));

  ConstBufferView src_region(src_file.bytes());
  ConstBufferView dst_region(dst_file.bytes());

  EnsemblePatchWriter patch_writer(src_region, dst_region);
  ASSERT_EQ(status::kStatusSuccess,
            GenerateBuffer(src_region, dst_region, &patch_writer));

  std::vector<uint8_t> patch_buffer(patch_writer.SerializedSize());
  ASSERT_TRUE(
      patch_writer.SerializeInto({patch_buffer.data(), patch_buffer.size()}));

  std::optional<EnsemblePatchReader> patch_reader =
      EnsemblePatchReader::Create({patch_buffer.data(), patch_buffer.size()});
  ASSERT_TRUE(patch_reader.has_value());
  EXPECT_TRUE(patch_reader->CheckOldFile(src_region));
  EXPECT_TRUE(patch_reader->CheckNewFile(dst_region));

  EXPECT_TRUE(std::ranges::any_of(
      patch_reader->elements(), [](const PatchElementReader& element) {
        return element.element_match().exe_type() == kExeTypeDex;
      }));

  std::vector<uint8_t> patched_buffer(dst_region.size());
  ASSERT_EQ(status::kStatusSuccess,
            ApplyBuffer(src_region, *patch_reader,
                        {patched_buffer.data(), patched_buffer.size()}));
  EXPECT_TRUE(std::ranges::equal(dst_region, patched_buffer));
}

}  // namespace

TEST(DexIntegrationTest, GenApplyDexV039Zip) {
  const base::FilePath testdata_dir =
      base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
          .AppendASCII("zucchini_testdata");
  const base::FilePath old_path = testdata_dir.AppendASCII("dex_v039_old.zip");
  const base::FilePath new_path = testdata_dir.AppendASCII("dex_v039_new.zip");

  {
    SCOPED_TRACE("old -> new");
    TestDexZipRoundTrip(old_path, new_path);
  }
  {
    SCOPED_TRACE("new -> old");
    TestDexZipRoundTrip(new_path, old_path);
  }
}

}  // namespace zucchini
