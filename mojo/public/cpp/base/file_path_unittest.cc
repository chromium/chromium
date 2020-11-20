// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace file_path_unittest {

TEST(FilePathTest, File) {
  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));
  base::FilePath file_out;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::FilePath>(file, file_out));
  ASSERT_EQ(file, file_out);
}

}  // namespace file_path_unittest
}  // namespace mojo_base
