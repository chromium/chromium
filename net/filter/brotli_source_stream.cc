// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/io_buffer.h"
#include "net/filter/source_stream_type.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "third_party/brotli/include/brotli/shared_dictionary.h"

namespace net {

namespace {

const char kBrotli[] = "BROTLI";

struct BrotliDecoderStateDeleter {
  void operator()(BrotliDecoderState* ptr) const {
    BrotliDecoderDestroyInstance(ptr);
  }
};

// BrotliSourceStream applies Brotli content decoding to a data stream.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli.
class BrotliSourceStream : public FilterSourceStream {
 public:
  explicit BrotliSourceStream(std::unique_ptr<SourceStream> upstream,
                              scoped_refptr<IOBuffer> dictionary = nullptr,
                              size_t dictionary_size = 0u)
      : FilterSourceStream(SourceStreamType::kBrotli, std::move(upstream)),
        dictionary_(std::move(dictionary)),
        dictionary_size_(dictionary_size),
        // The nullptrs mean the decoder will use malloc() and free() directly.
        brotli_state_(BrotliDecoderCreateInstance(nullptr, nullptr, nullptr)) {
    CHECK(brotli_state_);
    if (dictionary_) {
      BROTLI_BOOL result = BrotliDecoderAttachDictionary(
          brotli_state_.get(), BROTLI_SHARED_DICTIONARY_RAW, dictionary_size_,
          reinterpret_cast<const unsigned char*>(dictionary_->data()));
      CHECK(result);
    }
  }

  BrotliSourceStream(const BrotliSourceStream&) = delete;
  BrotliSourceStream& operator=(const BrotliSourceStream&) = delete;

  ~BrotliSourceStream() override {
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      // CompressionPercent is undefined when there is no output produced.
      if (produced_bytes_ != 0) {
        UMA_HISTOGRAM_PERCENTAGE(
            "BrotliFilter.CompressionPercent",
            static_cast<int>((consumed_bytes_ * 100) / produced_bytes_));
      }
    }
  }

 private:
  // Reported in UMA and must be kept in sync with the histograms.xml file.
  enum class DecodingStatus : int {
    DECODING_IN_PROGRESS = 0,
    DECODING_DONE,
    DECODING_ERROR,

    DECODING_STATUS_COUNT
    // DECODING_STATUS_COUNT must always be the last element in this enum.
  };

  // SourceStream implementation
  std::string GetTypeAsString() const override { return kBrotli; }

  base::expected<size_t, Error> FilterData(
      IOBuffer* output_buffer,
      size_t output_buffer_size,
      IOBuffer* input_buffer,
      size_t input_buffer_size,
      size_t* consumed_bytes,
      bool /*upstream_eof_reached*/) override {
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      *consumed_bytes = input_buffer_size;
      return 0;
    }

    if (decoding_status_ != DecodingStatus::DECODING_IN_PROGRESS)
      return base::unexpected(ERR_CONTENT_DECODING_FAILED);

    const uint8_t* next_in = reinterpret_cast<uint8_t*>(input_buffer->data());
    size_t available_in = input_buffer_size;
    uint8_t* next_out = reinterpret_cast<uint8_t*>(output_buffer->data());
    size_t available_out = output_buffer_size;

    BrotliDecoderResult result = BrotliDecoderDecompressStream(
        brotli_state_.get(), &available_in, &next_in, &available_out, &next_out,
        nullptr);

    size_t bytes_used = input_buffer_size - available_in;
    size_t bytes_written = output_buffer_size - available_out;
    CHECK_GE(input_buffer_size, available_in);
    CHECK_GE(output_buffer_size, available_out);
    produced_bytes_ += bytes_written;
    consumed_bytes_ += bytes_used;

    *consumed_bytes = bytes_used;

    switch (result) {
      case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
        return bytes_written;
      case BROTLI_DECODER_RESULT_SUCCESS:
        decoding_status_ = DecodingStatus::DECODING_DONE;
        // Consume remaining bytes to avoid DCHECK in FilterSourceStream.
        // See crbug.com/659311.
        *consumed_bytes = input_buffer_size;
        return bytes_written;
      case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
        // Decompress needs more input has consumed all existing input.
        DCHECK_EQ(*consumed_bytes, input_buffer_size);
        decoding_status_ = DecodingStatus::DECODING_IN_PROGRESS;
        return bytes_written;
      // If the decompressor threw an error, fail synchronously.
      default:
        decoding_status_ = DecodingStatus::DECODING_ERROR;
        return base::unexpected(ERR_CONTENT_DECODING_FAILED);
    }
  }

  const scoped_refptr<IOBuffer> dictionary_;
  const size_t dictionary_size_;

  std::unique_ptr<BrotliDecoderState, BrotliDecoderStateDeleter> brotli_state_;

  DecodingStatus decoding_status_ = DecodingStatus::DECODING_IN_PROGRESS;

  size_t consumed_bytes_ = 0;
  size_t produced_bytes_ = 0;
};

}  // namespace

std::unique_ptr<FilterSourceStream> CreateBrotliSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return std::make_unique<BrotliSourceStream>(std::move(previous));
}

std::unique_ptr<FilterSourceStream> CreateBrotliSourceStreamWithDictionary(
    std::unique_ptr<SourceStream> previous,
    scoped_refptr<IOBuffer> dictionary,
    size_t dictionary_size) {
  return std::make_unique<BrotliSourceStream>(
      std::move(previous), std::move(dictionary), dictionary_size);
}

}  // namespace net
