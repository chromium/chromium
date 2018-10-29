// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "net/third_party/quic/core/qpack/qpack_decoder_test_utils.h"
#include "net/third_party/quic/platform/api/quic_fuzzed_data_provider.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {
namespace test {
namespace {

class NoOpHeadersHandler : public QpackDecoder::HeadersHandlerInterface {
 public:
  ~NoOpHeadersHandler() override = default;

  void OnHeaderDecoded(QuicStringPiece name, QuicStringPiece value) override{};
  void OnDecodingCompleted() override{};
  void OnDecodingErrorDetected(QuicStringPiece error_message) override{};
};

}  // namespace

// This fuzzer exercises QpackDecoder.  It should be able to cover all possible
// code paths.  There is no point in encoding QpackDecoder's output to turn this
// into a roundtrip test, because the same header list can be encoded in many
// different ways, so the output could not be expected to match the original
// input.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  NoOpHeadersHandler handler;
  QuicFuzzedDataProvider provider(data, size);

  // Process up to 64 kB fragments at a time.  Too small upper bound might not
  // provide enough coverage, too large would make fuzzing less efficient.
  auto fragment_size_generator = std::bind(
      &QuicFuzzedDataProvider::ConsumeUint32InRange, &provider, 1, 64 * 1024);

  QpackDecode(
      &handler, fragment_size_generator,
      provider.ConsumeRandomLengthString(std::numeric_limits<size_t>::max()));

  return 0;
}

}  // namespace test
}  // namespace quic
