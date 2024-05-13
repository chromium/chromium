// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/socket/fuzzed_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/gurl.h"

// Fuzzer for HttpStreamParser.
//
// |data| is used to create a FuzzedSocket.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::TestCompletionCallback callback;
  // Including an observer; even though the recorded results aren't currently
  // used, it'll ensure the netlogging code is fuzzed as well.
  net::RecordingNetLogObserver net_log_observer;
  net::NetLogWithSource net_log_with_source =
      net::NetLogWithSource::Make(net::NetLogSourceType::NONE);
  FuzzedDataProvider data_provider(data, size);
  net::FuzzedSocket fuzzed_socket(&data_provider, net::NetLog::Get());
  CHECK_EQ(net::OK, fuzzed_socket.Connect(callback.callback()));

  scoped_refptr<net::GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  // Use a NetLog that listens to events, to get coverage of logging
  // callbacks.
  net::HttpStreamParser parser(
      &fuzzed_socket, false /* is_reused */, GURL("http://localhost/"), "GET",
      /*upload_data_stream=*/nullptr, read_buffer.get(), net_log_with_source);

  net::HttpResponseInfo response_info;
  int result = parser.SendRequest(
      "GET / HTTP/1.1\r\n", net::HttpRequestHeaders(),
      TRAFFIC_ANNOTATION_FOR_TESTS, &response_info, callback.callback());
  result = callback.GetResult(result);
  if (net::OK != result)
    return 0;

  result = parser.ReadResponseHeaders(callback.callback());
  result = callback.GetResult(result);

  if (result < 0)
    return 0;

  while (true) {
    scoped_refptr<net::IOBufferWithSize> io_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(64);
    result = parser.ReadResponseBody(io_buffer.get(), io_buffer->size(),
                                     callback.callback());

    // Releasing the pointer to IOBuffer immediately is more likely to lead to a
    // use-after-free.
    io_buffer = nullptr;
    if (callback.GetResult(result) <= 0)
      break;
  }

  return 0;
}
