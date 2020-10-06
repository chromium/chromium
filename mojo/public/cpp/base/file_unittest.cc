// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/file.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace file_unittest {

TEST(FileTest, File) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::File file(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  const base::StringPiece test_content =
      "A test string to be stored in a test file";
  file.WriteAtCurrentPos(test_content.data(),
                         base::checked_cast<int>(test_content.size()));

  base::File file_out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::File>(&file, &file_out));
  std::vector<char> content(test_content.size());
  ASSERT_TRUE(file_out.IsValid());
  ASSERT_FALSE(file_out.async());
  ASSERT_EQ(static_cast<int>(test_content.size()),
            file_out.Read(0, content.data(),
                          base::checked_cast<int>(test_content.size())));
  EXPECT_EQ(test_content,
            base::StringPiece(content.data(), test_content.size()));
}

TEST(FileTest, AsyncFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("async_test_file.txt");

  base::File write_file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  const base::StringPiece test_content = "test string";
  write_file.WriteAtCurrentPos(test_content.data(),
                               base::checked_cast<int>(test_content.size()));
  write_file.Close();

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_ASYNC);
  base::File file_out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::File>(&file, &file_out));
  ASSERT_TRUE(file_out.async());
}

TEST(FileTest, InvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Test that |file_out| is set to an invalid file.
  base::File file_out(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);

  base::File file = base::File();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::File>(&file, &file_out));
  EXPECT_FALSE(file_out.IsValid());
}

}  // namespace file_unittest
}  // namespace mojo_base
