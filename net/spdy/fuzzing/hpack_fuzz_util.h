// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_FUZZING_HPACK_FUZZ_UTIL_H_
#define NET_SPDY_FUZZING_HPACK_FUZZ_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "net/third_party/quiche/src/quiche/http2/core/recording_headers_handler.h"
#include "net/third_party/quiche/src/quiche/http2/hpack/hpack_decoder_adapter.h"
#include "net/third_party/quiche/src/quiche/http2/hpack/hpack_encoder.h"

namespace quiche {
class HttpHeaderBlock;
}

namespace spdy {

class HpackFuzzUtil {
 public:
  // A GeneratorContext holds ordered header names & values which are
  // initially seeded and then expanded with dynamically generated data.
  struct GeneratorContext {
    GeneratorContext();
    ~GeneratorContext();
    std::vector<std::string> names;
    std::vector<std::string> values;
  };

  // Initializes a GeneratorContext with a random seed and name/value fixtures.
  static void InitializeGeneratorContext(GeneratorContext* context);

  // Generates a header set from the generator context.
  static quiche::HttpHeaderBlock NextGeneratedHeaderSet(
      GeneratorContext* context);

  // Samples a size from the exponential distribution with mean |mean|,
  // upper-bounded by |sanity_bound|.
  static size_t SampleExponential(size_t mean, size_t sanity_bound);

  // Holds an input string, and manages an offset into that string.
  struct Input {
    Input();  // Initializes |offset| to zero.
    ~Input();

    // Returns a span over the next `bytes` many characters in the buffer, and
    // advances the buffer offset past them.
    base::span<const uint8_t> ReadSpan(size_t bytes) {
      auto out = RemainingBytes().first(bytes);
      offset += bytes;
      return out;
    }
    // Returns a span over the next `bytes` many characters in the buffer, and
    // advances the buffer offset past them.
    //
    // This version takes a compile-time size and returns a fixed-size span.
    template <size_t bytes>
    base::span<const uint8_t, bytes> ReadSpan() {
      auto out = RemainingBytes().first<bytes>();
      offset += bytes;
      return out;
    }

    // Returns a span over all remaining bytes in the input buffer.
    base::span<const uint8_t> RemainingBytes() {
      return base::as_byte_span(input).subspan(offset);
    }

    std::string input;
    size_t offset = 0;
  };

  // Returns true if the next header block was set at |out|. Returns
  // false if no input header blocks remain.
  static bool NextHeaderBlock(Input* input, std::string_view* out);

  // Returns the serialized header block length prefix for a block of
  // |block_size| bytes.
  static std::string HeaderBlockPrefix(size_t block_size);

  // A FuzzerContext holds fuzzer input, as well as each of the decoder and
  // encoder stages which fuzzed header blocks are processed through.
  struct FuzzerContext {
    FuzzerContext();
    ~FuzzerContext();
    std::unique_ptr<HpackDecoderAdapter> first_stage;
    std::unique_ptr<RecordingHeadersHandler> first_stage_handler;
    std::unique_ptr<HpackEncoder> second_stage;
    std::unique_ptr<HpackDecoderAdapter> third_stage;
    std::unique_ptr<RecordingHeadersHandler> third_stage_handler;
  };

  static void InitializeFuzzerContext(FuzzerContext* context);

  // Runs |input_block| through |first_stage| and, iff that succeeds,
  // |second_stage| and |third_stage| as well. Returns whether all stages
  // processed the input without error.
  static bool RunHeaderBlockThroughFuzzerStages(FuzzerContext* context,
                                                std::string_view input_block);

  // Flips random bits within |buffer|. The total number of flips is
  // |flip_per_thousand| bits for every 1,024 bytes of |buffer_length|,
  // rounding up.
  static void FlipBits(uint8_t* buffer,
                       size_t buffer_length,
                       size_t flip_per_thousand);
};

}  // namespace spdy

#endif  // NET_SPDY_FUZZING_HPACK_FUZZ_UTIL_H_
