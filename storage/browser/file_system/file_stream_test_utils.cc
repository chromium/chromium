// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_stream_test_utils.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

base::expected<std::string, net::Error> ReadFromReader(FileStreamReader& reader,
                                                       size_t bytes_to_read) {
  std::string result;
  size_t total_bytes_read = 0;
  while (total_bytes_read < bytes_to_read) {
    scoped_refptr<net::IOBufferWithSize> buf(
        base::MakeRefCounted<net::IOBufferWithSize>(bytes_to_read -
                                                    total_bytes_read));
    net::TestCompletionCallback callback;
    int rv = reader.Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    if (rv < 0) {
      return base::unexpected(static_cast<net::Error>(rv));
    } else if (rv == 0) {
      break;
    }
    total_bytes_read += rv;
    result.append(buf->data(), rv);
  }
  return result;
}

base::expected<int64_t, net::Error> GetLengthFromReader(
    FileStreamReader* reader) {
  EXPECT_NE(nullptr, reader);

  base::expected<int64_t, net::Error> result;
  base::RunLoop run_loop;
  int64_t rv = reader->GetLength(base::BindOnce(
      [](base::expected<int64_t, net::Error>* out_result,
         base::OnceClosure quit_closure,
         base::expected<int64_t, net::Error> length) {
        *out_result = length;
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  if (rv != net::ERR_IO_PENDING) {
    if (rv < 0) {
      return base::unexpected(static_cast<net::Error>(rv));
    }
    return rv;
  }
  run_loop.Run();
  return result;
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
