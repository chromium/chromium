// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "net/socket/transport_client_socket_test_util.h"

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

void SendRequestAndResponse(StreamSocket* socket,
                            StreamSocket* connected_socket) {
  // Send client request.
  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  int request_len = strlen(request_text);
  scoped_refptr<DrainableIOBuffer> request_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<IOBufferWithSize>(request_len), request_len);
  memcpy(request_buffer->data(), request_text, request_len);

  int bytes_written = 0;
  while (request_buffer->BytesRemaining() > 0) {
    TestCompletionCallback write_callback;
    int write_result =
        socket->Write(request_buffer.get(), request_buffer->BytesRemaining(),
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);

    write_result = write_callback.GetResult(write_result);
    ASSERT_GT(write_result, 0);
    ASSERT_LE(bytes_written + write_result, request_len);
    request_buffer->DidConsume(write_result);

    bytes_written += write_result;
  }
  ASSERT_EQ(request_len, bytes_written);

  // Confirm that the server receives what client sent.
  std::string data_received =
      ReadDataOfExpectedLength(connected_socket, bytes_written);
  ASSERT_TRUE(connected_socket->IsConnectedAndIdle());
  ASSERT_EQ(request_text, data_received);

  // Write server response.
  SendServerResponse(connected_socket);
}

std::string ReadDataOfExpectedLength(StreamSocket* socket,
                                     int expected_bytes_read) {
  int bytes_read = 0;
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(expected_bytes_read);
  while (bytes_read < expected_bytes_read) {
    TestCompletionCallback read_callback;
    int rv = socket->Read(read_buffer.get(), expected_bytes_read - bytes_read,
                          read_callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    rv = read_callback.GetResult(rv);
    EXPECT_GE(rv, 0);
    bytes_read += rv;
  }
  EXPECT_EQ(expected_bytes_read, bytes_read);
  return std::string(read_buffer->data(), bytes_read);
}

void SendServerResponse(StreamSocket* socket) {
  const char kServerReply[] = "HTTP/1.1 404 Not Found";
  int reply_len = strlen(kServerReply);
  scoped_refptr<DrainableIOBuffer> write_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<IOBufferWithSize>(reply_len), reply_len);
  memcpy(write_buffer->data(), kServerReply, reply_len);
  int bytes_written = 0;
  while (write_buffer->BytesRemaining() > 0) {
    TestCompletionCallback write_callback;
    int write_result =
        socket->Write(write_buffer.get(), write_buffer->BytesRemaining(),
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    write_result = write_callback.GetResult(write_result);
    ASSERT_GE(write_result, 0);
    ASSERT_LE(bytes_written + write_result, reply_len);
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
