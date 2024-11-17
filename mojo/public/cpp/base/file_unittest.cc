// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sync_socket.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/read_only_file_mojom_traits.h"
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
  const std::string_view test_content =
      "A test string to be stored in a test file";
  file.WriteAtCurrentPos(base::as_byte_span(test_content));

  base::File file_out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::File>(file, file_out));
  ASSERT_TRUE(file_out.IsValid());
  ASSERT_FALSE(file_out.async());

  std::string content(test_content.size(), '\0');
  ASSERT_TRUE(file_out.ReadAndCheck(0, base::as_writable_byte_span(content)));
  EXPECT_EQ(test_content, content);
}

TEST(FileTest, AsyncFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("async_test_file.txt");

  base::File write_file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  const std::string_view test_content = "test string";
  write_file.WriteAtCurrentPos(base::as_byte_span(test_content));
  write_file.Close();

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_ASYNC);
  base::File file_out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::File>(file, file_out));
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
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::File>(file, file_out));
  EXPECT_FALSE(file_out.IsValid());
}

TEST(FileTest, ReadOnlyFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::File file(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  const std::string_view test_content =
      "A test string to be stored in a test file";
  file.WriteAtCurrentPos(base::as_byte_span(test_content));
  file.Close();

  base::File readonly(temp_dir.GetPath().AppendASCII("test_file.txt"),
                      base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::File file_out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ReadOnlyFile>(
      readonly, file_out));
  ASSERT_TRUE(file_out.IsValid());
  ASSERT_FALSE(file_out.async());

  std::string content(test_content.size(), '\0');
  ASSERT_TRUE(file_out.ReadAndCheck(0, base::as_writable_byte_span(content)));
  EXPECT_EQ(test_content, content);
}

// This dies only if we can interrogate the underlying platform handle.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
TEST(FileTest, ReadOnlyFileDeath) {
#if defined(OFFICIAL_BUILD)
  const char kReadOnlyFileCheckFailedRegex[] = "";
#else
  const char kReadOnlyFileCheckFailedRegex[] = "Check failed: IsReadOnlyFile";
#endif

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::File file(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  const std::string_view test_content =
      "A test string to be stored in a test file";
  file.WriteAtCurrentPos(base::as_byte_span(test_content));
  file.Close();

  base::File writable(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);

  base::File file_out;
  EXPECT_DEATH_IF_SUPPORTED(
      mojo::test::SerializeAndDeserialize<mojom::ReadOnlyFile>(writable,
                                                               file_out),
      kReadOnlyFileCheckFailedRegex);
}
#endif  // !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// This should work on all platforms. This check might be relaxed in which case
// this test can be removed. iOS without blink does not build SyncSocket, so do
// not build this when blink isn't used.
#if DCHECK_IS_ON() && (!BUILDFLAG(IS_IOS) || BUILDFLAG(MOJO_USE_APPLE_CHANNEL))
TEST(FileTest, NonPhysicalFileDeath) {
#if defined(OFFICIAL_BUILD)
  const char kPhysicalFileCheckFailedRegex[] = "";
#else
  const char kPhysicalFileCheckFailedRegex[] = "Check failed: IsPhysicalFile";
#endif

  base::SyncSocket sync_a;
  base::SyncSocket sync_b;
  ASSERT_TRUE(base::SyncSocket::CreatePair(&sync_a, &sync_b));
  base::File file_pipe_a(sync_a.Take());
  base::File file_pipe_b(sync_b.Take());

  base::File file_out;
  EXPECT_DEATH_IF_SUPPORTED(
      mojo::test::SerializeAndDeserialize<mojom::ReadOnlyFile>(file_pipe_a,
                                                               file_out),
      kPhysicalFileCheckFailedRegex);
  EXPECT_DEATH_IF_SUPPORTED(
      mojo::test::SerializeAndDeserialize<mojom::ReadOnlyFile>(file_pipe_b,
                                                               file_out),
      kPhysicalFileCheckFailedRegex);
}
#endif  // DCHECK_IS_ON()

}  // namespace file_unittest
}  // namespace mojo_base
