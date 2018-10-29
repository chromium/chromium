// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_SPDY_TEST_UTILS_H_
#define NET_THIRD_PARTY_SPDY_CORE_SPDY_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "net/test/gtest_util.h"
#include "net/third_party/http2/tools/failure.h"
#include "net/third_party/spdy/core/spdy_bug_tracker.h"
#include "net/third_party/spdy/core/spdy_header_block.h"
#include "net/third_party/spdy/core/spdy_headers_handler_interface.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/platform/api/spdy_string.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

#define EXPECT_SPDY_BUG EXPECT_DFATAL

namespace spdy {

inline bool operator==(SpdyStringPiece x,
                       const SpdyHeaderBlock::ValueProxy& y) {
  return x == y.as_string();
}

namespace test {

SpdyString HexDumpWithMarks(const unsigned char* data,
                            int length,
                            const bool* marks,
                            int mark_length);

void CompareCharArraysWithHexError(const SpdyString& description,
                                   const unsigned char* actual,
                                   const int actual_len,
                                   const unsigned char* expected,
                                   const int expected_len);

void SetFrameFlags(SpdySerializedFrame* frame, uint8_t flags);

void SetFrameLength(SpdySerializedFrame* frame, size_t length);

// A test implementation of SpdyHeadersHandlerInterface that correctly
// reconstructs multiple header values for the same name.
class TestHeadersHandler : public SpdyHeadersHandlerInterface {
 public:
  TestHeadersHandler() {}
  TestHeadersHandler(const TestHeadersHandler&) = delete;
  TestHeadersHandler& operator=(const TestHeadersHandler&) = delete;

  void OnHeaderBlockStart() override;

  void OnHeader(SpdyStringPiece name, SpdyStringPiece value) override;

  void OnHeaderBlockEnd(size_t header_bytes_parsed,
                        size_t compressed_header_bytes_parsed) override;

  const SpdyHeaderBlock& decoded_block() const { return block_; }
  size_t header_bytes_parsed() const { return header_bytes_parsed_; }
  size_t compressed_header_bytes_parsed() const {
    return compressed_header_bytes_parsed_;
  }

 private:
  SpdyHeaderBlock block_;
  size_t header_bytes_parsed_ = 0;
  size_t compressed_header_bytes_parsed_ = 0;
};

}  // namespace test
}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_SPDY_TEST_UTILS_H_
