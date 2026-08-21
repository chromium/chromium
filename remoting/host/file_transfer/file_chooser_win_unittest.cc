// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/message.h"
#include "remoting/host/file_transfer/file_chooser_common_win.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class FileChooserWinTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FileChooserWinTest, ParseValidSuccessResponse) {
  base::FilePath test_path(FILE_PATH_LITERAL("C:\\test\\file.txt"));
  FileChooser::Result input(test_path);
  mojo::Message serialized_message =
      mojom::FileChooserResult::SerializeAsMessage(&input);

  FileChooser::Result result =
      ParseFileChooserResponse(serialized_message.data_as_span());

  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.success(), test_path);
}

TEST_F(FileChooserWinTest, ParseValidErrorResponse) {
  protocol::FileTransfer_Error error = MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED);
  FileChooser::Result input(error);
  mojo::Message serialized_message =
      mojom::FileChooserResult::SerializeAsMessage(&input);

  FileChooser::Result result =
      ParseFileChooserResponse(serialized_message.data_as_span());

  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(), protocol::FileTransfer_Error_Type_CANCELED);
}

TEST_F(FileChooserWinTest, RejectMalformedHeaderWithInvalidNumBytes) {
  // Vector A: 8-byte V0 header with num_bytes (0x28 = 40) larger than buffer.
  const uint8_t malformed_bytes[8] = {0x28, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
  FileChooser::Result result = ParseFileChooserResponse(malformed_bytes);

  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(),
            protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
}

TEST_F(FileChooserWinTest, RejectMalformedHeaderWithV2PayloadPointerOOB) {
  // Vector B: 48-byte V2 header with payload offset pointing outside buffer.
  uint8_t malformed_bytes[48] = {0};
  // num_bytes = 48
  malformed_bytes[0] = 48;
  // version = 2
  malformed_bytes[4] = 2;
  // payload offset pointing 0x1000 bytes away
  malformed_bytes[32] = 0x00;
  malformed_bytes[33] = 0x10;
  malformed_bytes[40] = 0x00;
  malformed_bytes[41] = 0x10;

  FileChooser::Result result = ParseFileChooserResponse(malformed_bytes);

  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(),
            protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
}

TEST_F(FileChooserWinTest, RejectEmptyBuffer) {
  FileChooser::Result result =
      ParseFileChooserResponse(base::span<const uint8_t>());

  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(),
            protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
}

TEST_F(FileChooserWinTest, RejectTruncatedHeaders) {
  // Test various truncated buffer sizes smaller than full Mojo message headers.
  for (size_t size : {1, 4, 7, 16, 24}) {
    std::vector<uint8_t> truncated_bytes(size, 0);
    FileChooser::Result result = ParseFileChooserResponse(truncated_bytes);

    ASSERT_TRUE(result.is_error()) << "Failed for size: " << size;
    EXPECT_EQ(result.error().type(),
              protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR)
        << "Failed for size: " << size;
  }
}

}  // namespace remoting
