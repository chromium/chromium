// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/test_helpers.h"
#include "media/filters/file_data_source.h"

using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

class ReadCBHandler {
 public:
  ReadCBHandler() = default;

  ReadCBHandler(const ReadCBHandler&) = delete;
  ReadCBHandler& operator=(const ReadCBHandler&) = delete;

  MOCK_METHOD1(ReadCB, void(int size));
};

// Returns a path to the test file which contains the string "0123456789"
// without the quotes or any trailing space or null termination.  The file lives
// under the media/test/data directory.  Under Windows, strings for the
// FilePath class are unicode, and the pipeline wants char strings.  Convert
// the string to UTF8 under Windows.  For Mac and Linux, file paths are already
// chars so just return the string from the base::FilePath.
base::FilePath TestFileURL() {
  base::FilePath data_dir;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("media"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("ten_byte_file"));
  return data_dir;
}

// Use the mock filter host to directly call the Read and GetPosition methods.
TEST(FileDataSourceTest, ReadData) {
  int64_t size;
  uint8_t ten_bytes[10];

  // Create our mock filter host and initialize the data source.
  FileDataSource data_source;

  EXPECT_TRUE(data_source.Initialize(TestFileURL()));
  EXPECT_TRUE(data_source.GetSize(&size));
  EXPECT_EQ(10, size);

  ReadCBHandler handler;
  EXPECT_CALL(handler, ReadCB(10));
  data_source.Read(
      0, 10, ten_bytes,
      base::BindOnce(&ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('0', ten_bytes[0]);
  EXPECT_EQ('5', ten_bytes[5]);
  EXPECT_EQ('9', ten_bytes[9]);

  EXPECT_CALL(handler, ReadCB(1));
  data_source.Read(
      9, 1, ten_bytes,
      base::BindOnce(&ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('9', ten_bytes[0]);

  EXPECT_CALL(handler, ReadCB(0));
  data_source.Read(
      10, 10, ten_bytes,
      base::BindOnce(&ReadCBHandler::ReadCB, base::Unretained(&handler)));

  EXPECT_CALL(handler, ReadCB(5));
  data_source.Read(
      5, 10, ten_bytes,
      base::BindOnce(&ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('5', ten_bytes[0]);

  data_source.Stop();
}

}  // namespace media
