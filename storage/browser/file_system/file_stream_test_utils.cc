// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_stream_test_utils.h"

#include <string>
#include <utility>

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

void ReadFromReader(FileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result) {
  ASSERT_TRUE(reader != nullptr);
  ASSERT_TRUE(result != nullptr);
  *result = net::OK;
  size_t total_bytes_read = 0;
  while (total_bytes_read < size) {
    scoped_refptr<net::IOBufferWithSize> buf(
        base::MakeRefCounted<net::IOBufferWithSize>(size - total_bytes_read));
    net::TestCompletionCallback callback;
    int rv = reader->Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      *result = rv;
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data->append(buf->data(), rv);
  }
}

int WriteStringToWriter(FileStreamWriter* writer, const std::string& data) {
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(data);
  scoped_refptr<net::DrainableIOBuffer> drainable =
      base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer),
                                                   data.size());
  while (drainable->BytesRemaining() > 0) {
    net::TestCompletionCallback callback;
    int result = writer->Write(drainable.get(), drainable->BytesRemaining(),
                               callback.callback());
    if (result == net::ERR_IO_PENDING)
      result = callback.WaitForResult();
    if (result <= 0)
      return result;
    drainable->DidConsume(result);
  }
  return net::OK;
}

}  // namespace storage
