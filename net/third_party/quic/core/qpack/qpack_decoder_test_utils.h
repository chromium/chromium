// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_

#include "net/third_party/quic/core/qpack/qpack_decoder.h"
#include "net/third_party/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/spdy/core/spdy_header_block.h"

namespace quic {
namespace test {

// HeadersHandlerInterface implementation that collects decoded headers
// into a SpdyHeaderBlock.
class TestHeadersHandler : public QpackDecoder::HeadersHandlerInterface {
 public:
  TestHeadersHandler();

  // HeadersHandlerInterface implementation:
  void OnHeaderDecoded(QuicStringPiece name, QuicStringPiece value) override;
  void OnDecodingCompleted() override;
  void OnDecodingErrorDetected(QuicStringPiece error_message) override;

  spdy::SpdyHeaderBlock ReleaseHeaderList();
  bool decoding_completed() const;
  bool decoding_error_detected() const;

 private:
  spdy::SpdyHeaderBlock header_list_;
  bool decoding_completed_;
  bool decoding_error_detected_;
};

void QpackDecode(QpackDecoder::HeadersHandlerInterface* handler,
                 const FragmentSizeGenerator& fragment_size_generator,
                 QuicStringPiece data);

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_
