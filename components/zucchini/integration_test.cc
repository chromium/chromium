// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/zucchini.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

base::FilePath MakeTestPath(const std::string& filename) {
  base::FilePath path;
  DCHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
  return path.AppendASCII("components")
      .AppendASCII("zucchini")
      .AppendASCII("testdata")
      .AppendASCII(filename);
}

void TestGenApply(const std::string& old_filename,
                  const std::string& new_filename,
                  bool raw) {
  base::FilePath old_path = MakeTestPath(old_filename);
  base::FilePath new_path = MakeTestPath(new_filename);

  base::MemoryMappedFile old_file;
  ASSERT_TRUE(old_file.Initialize(old_path));

  base::MemoryMappedFile new_file;
  ASSERT_TRUE(new_file.Initialize(new_path));

  ConstBufferView old_region(old_file.data(), old_file.length());
  ConstBufferView new_region(new_file.data(), new_file.length());

  EnsemblePatchWriter patch_writer(old_region, new_region);

  // Generate patch from "old" to "new".
  ASSERT_EQ(status::kStatusSuccess,
            raw ? GenerateBufferRaw(old_region, new_region, &patch_writer)
                : GenerateBuffer(old_region, new_region, &patch_writer));

  size_t patch_size = patch_writer.SerializedSize();
  EXPECT_GE(patch_size, 80U);  // Minimum size is empty patch.
  // TODO(etiennep): Add check on maximum expected size.

  std::vector<uint8_t> patch_buffer(patch_writer.SerializedSize());
  patch_writer.SerializeInto({patch_buffer.data(), patch_buffer.size()});

  // Read back generated patch.
  std::optional<EnsemblePatchReader> patch_reader =
      EnsemblePatchReader::Create({patch_buffer.data(), patch_buffer.size()});
  ASSERT_TRUE(patch_reader.has_value());

  // Check basic properties.
  EXPECT_TRUE(patch_reader->CheckOldFile(old_region));
  EXPECT_TRUE(patch_reader->CheckNewFile(new_region));
  EXPECT_EQ(old_file.length(), patch_reader->header().old_size);
  // If new_size doesn't match expectation, the function is aborted.
  ASSERT_EQ(new_file.length(), patch_reader->header().new_size);

  // Apply patch to "old" to get "patched new", ensure it's identical to "new".
  std::vector<uint8_t> patched_new_buffer(new_region.size());
  ASSERT_EQ(status::kStatusSuccess, ApplyBuffer(old_region, *patch_reader,
                                                {patched_new_buffer.data(),
                                                 patched_new_buffer.size()}));

  // Note that |new_region| and |patched_new_buffer| are the same size.
  EXPECT_TRUE(base::ranges::equal(new_region, patched_new_buffer));
}

TEST(EndToEndTest, GenApplyRaw) {
  TestGenApply("setup1.exe", "setup2.exe", true);
  TestGenApply("chrome64_1.exe", "chrome64_2.exe", true);
}

TEST(EndToEndTest, GenApplyIdentity) {
  TestGenApply("setup1.exe", "setup1.exe", false);
}

TEST(EndToEndTest, GenApplySimple) {
  TestGenApply("setup1.exe", "setup2.exe", false);
  TestGenApply("setup2.exe", "setup1.exe", false);
  TestGenApply("chrome64_1.exe", "chrome64_2.exe", false);
}

TEST(EndToEndTest, GenApplyCross) {
  TestGenApply("setup1.exe", "chrome64_1.exe", false);
}

}  // namespace zucchini
