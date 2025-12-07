// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_test_util.h"

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_view_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

void SendRequestAndResponse(StreamSocket* socket,
                            StreamSocket* connected_socket) {
  // Send client request.
  const std::string kRequestText = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<DrainableIOBuffer> request_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<StringIOBuffer>(kRequestText),
          kRequestText.size());

  while (request_buffer->BytesRemaining() > 0) {
    TestCompletionCallback write_callback;
    int write_result =
        socket->Write(request_buffer.get(), request_buffer->BytesRemaining(),
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);

    write_result = write_callback.GetResult(write_result);
    ASSERT_GT(write_result, 0);
    ASSERT_LE(write_result, request_buffer->size());
    request_buffer->DidConsume(write_result);
  }

  // Confirm that the server receives what client sent.
  std::string data_received =
      ReadDataOfExpectedLength(connected_socket, kRequestText.length());
  ASSERT_TRUE(connected_socket->IsConnectedAndIdle());
  ASSERT_EQ(kRequestText, data_received);

  // Write server response.
  SendServerResponse(connected_socket);
}

std::string ReadDataOfExpectedLength(StreamSocket* socket,
                                     int expected_bytes_read) {
  auto read_buffer = base::MakeRefCounted<DrainableIOBuffer>(
      base::MakeRefCounted<IOBufferWithSize>(expected_bytes_read),
      expected_bytes_read);
  while (read_buffer->size() > 0) {
    TestCompletionCallback read_callback;
    int rv = socket->Read(read_buffer.get(), read_buffer->size(),
                          read_callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    rv = read_callback.GetResult(rv);
    CHECK_GE(rv, 0);
    EXPECT_LE(rv, read_buffer->size());
    read_buffer->DidConsume(rv);
  }
  read_buffer->SetOffset(0);
  return std::string(base::as_string_view(read_buffer->span()));
}

void SendServerResponse(StreamSocket* socket) {
  const std::string kServerReply = "HTTP/1.1 404 Not Found";
  scoped_refptr<DrainableIOBuffer> write_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<StringIOBuffer>(kServerReply),
          kServerReply.size());
  int bytes_written = 0;
  while (write_buffer->BytesRemaining() > 0) {
    TestCompletionCallback write_callback;
    int write_result =
        socket->Write(write_buffer.get(), write_buffer->BytesRemaining(),
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    write_result = write_callback.GetResult(write_result);
    ASSERT_GE(write_result, 0);
    ASSERT_LE(bytes_written + write_result,
              static_cast<int>(kServerReply.size()));
    write_buffer->DidConsume(write_result);
    bytes_written += write_result;
  }
}

int DrainStreamSocket(StreamSocket* socket,
                      IOBuffer* buf,
                      uint32_t buf_len,
                      uint32_t bytes_to_read,
                      TestCompletionCallback* callback) {
  int rv = OK;
  uint32_t bytes_read = 0;

  while (bytes_read < bytes_to_read) {
    rv = socket->Read(buf, buf_len, callback->callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    rv = callback->GetResult(rv);
    EXPECT_GT(rv, 0);
    bytes_read += rv;
  }

  return static_cast<int>(bytes_read);
}

}  // namespace net
