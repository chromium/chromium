// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include <array>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace blink {

class FileSystemAccessRegularFileDelegateTest : public PageTestBase {};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(FileSystemAccessRegularFileDelegateTest, WriteReturnsNoSpaceError) {
  base::File backing_file(base::FilePath(FILE_PATH_LITERAL("/dev/full")),
                          base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(backing_file.IsValid());

  mojo::PendingRemote<mojom::blink::FileSystemAccessFileModificationHost>
      modification_host_remote;
  auto modification_host_receiver =
      modification_host_remote.InitWithNewPipeAndPassReceiver();
  ASSERT_TRUE(modification_host_receiver.is_valid());

  // Seed one allocation block so the capacity tracker does not issue a
  // synchronous Mojo request before the write reaches /dev/full.
  auto regular_file = mojom::blink::FileSystemAccessRegularFile::New(
      std::move(backing_file), 1024 * 1024,
      std::move(modification_host_remote));
  FileSystemAccessFileDelegate* delegate = FileSystemAccessFileDelegate::Create(
      GetDocument().GetExecutionContext(), std::move(regular_file));

  const std::array<uint8_t, 1> data = {0};
  base::FileErrorOr<int> result = delegate->Write(0, data);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, result.error());
}

TEST_F(FileSystemAccessRegularFileDelegateTest, GetLengthReturnsFileError) {
  mojo::PendingRemote<mojom::blink::FileSystemAccessFileModificationHost>
      modification_host_remote;
  auto modification_host_receiver =
      modification_host_remote.InitWithNewPipeAndPassReceiver();
  ASSERT_TRUE(modification_host_receiver.is_valid());

  base::File valid_file(base::FilePath(FILE_PATH_LITERAL("/dev/null")),
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(valid_file.IsValid());
  const base::PlatformFile closed_fd = valid_file.TakePlatformFile();
  close(closed_fd);

  // Preserve the closed descriptor number so base::File::GetLength() reaches
  // fstat() and fails deterministically with EBADF.
  base::File backing_file(closed_fd);
  ASSERT_TRUE(backing_file.IsValid());
  auto regular_file = mojom::blink::FileSystemAccessRegularFile::New(
      std::move(backing_file), 0, std::move(modification_host_remote));
  FileSystemAccessFileDelegate* delegate = FileSystemAccessFileDelegate::Create(
      GetDocument().GetExecutionContext(), std::move(regular_file));
  ASSERT_TRUE(delegate->IsValid());

  base::FileErrorOr<int64_t> result = delegate->GetLength();
  const int get_length_errno = errno;

  EXPECT_EQ(EBADF, get_length_errno);
  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            base::File::OSErrorToFileError(get_length_errno));
  EXPECT_FALSE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(base::File::FILE_ERROR_FAILED, result.value());
  } else {
    EXPECT_EQ(base::File::FILE_ERROR_FAILED, result.error());
  }

  // Reopen the descriptor before the delegate closes it so ScopedFD's
  // ownership check does not treat the intentionally stale descriptor as a
  // double close.
  ASSERT_EQ(closed_fd, open("/dev/null", O_RDONLY));
  delegate->Close();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace blink
