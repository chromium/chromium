// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/io_buffer.h"
#include "third_party/brotli/include/brotli/decode.h"

namespace net {

namespace {

const char kBrotli[] = "BROTLI";

// BrotliSourceStream applies Brotli content decoding to a data stream.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli.
class BrotliSourceStream : public FilterSourceStream {
 public:
  explicit BrotliSourceStream(std::unique_ptr<SourceStream> upstream)
      : FilterSourceStream(SourceStream::TYPE_BROTLI, std::move(upstream)),
        decoding_status_(DecodingStatus::DECODING_IN_PROGRESS),
        used_memory_(0),
        used_memory_maximum_(0),
        consumed_bytes_(0),
        produced_bytes_(0) {
    brotli_state_ =
        BrotliDecoderCreateInstance(AllocateMemory, FreeMemory, this);
    CHECK(brotli_state_);
  }

  ~BrotliSourceStream() override {
    BrotliDecoderErrorCode error_code =
        BrotliDecoderGetErrorCode(brotli_state_);
    BrotliDecoderDestroyInstance(brotli_state_);
    brotli_state_ = nullptr;
    DCHECK_EQ(0u, used_memory_);


    UMA_HISTOGRAM_ENUMERATION(
        "BrotliFilter.Status", static_cast<int>(decoding_status_),
        static_cast<int>(DecodingStatus::DECODING_STATUS_COUNT));
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      // CompressionPercent is undefined when there is no output produced.
      if (produced_bytes_ != 0) {
        UMA_HISTOGRAM_PERCENTAGE(
            "BrotliFilter.CompressionPercent",
            static_cast<int>((consumed_bytes_ * 100) / produced_bytes_));
      }
    }
    if (error_code < 0) {
      UMA_HISTOGRAM_ENUMERATION("BrotliFilter.ErrorCode",
                                -static_cast<int>(error_code),
                                1 - BROTLI_LAST_ERROR_CODE);
    }

    // All code here is for gathering stats, and can be removed when
    // BrotliSourceStream is considered stable.
    const int kBuckets = 48;
    const int64_t kMaxKb = 1 << (kBuckets / 3);  // 64MiB in KiB
    UMA_HISTOGRAM_CUSTOM_COUNTS("BrotliFilter.UsedMemoryKB",
                                used_memory_maximum_ / 1024, 1, kMaxKb,
                                kBuckets);
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

  int FilterData(IOBuffer* output_buffer,
                 int output_buffer_size,
                 IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool /*upstream_eof_reached*/) override {
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      *consumed_bytes = input_buffer_size;
      return OK;
    }

    if (decoding_status_ != DecodingStatus::DECODING_IN_PROGRESS)
      return ERR_CONTENT_DECODING_FAILED;

    const uint8_t* next_in = bit_cast<uint8_t*>(input_buffer->data());
    size_t available_in = input_buffer_size;
    uint8_t* next_out = bit_cast<uint8_t*>(output_buffer->data());
    size_t available_out = output_buffer_size;

    BrotliDecoderResult result =
        BrotliDecoderDecompressStream(brotli_state_, &available_in, &next_in,
                                      &available_out, &next_out, nullptr);

    size_t bytes_used = input_buffer_size - available_in;
    size_t bytes_written = output_buffer_size - available_out;
    CHECK_GE(bytes_used, 0u);
    CHECK_GE(bytes_written, 0u);
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
        return ERR_CONTENT_DECODING_FAILED;
    }
  }

  static void* AllocateMemory(void* opaque, size_t size) {
    BrotliSourceStream* filter = reinterpret_cast<BrotliSourceStream*>(opaque);
    return filter->AllocateMemoryInternal(size);
  }

  static void FreeMemory(void* opaque, void* address) {
    BrotliSourceStream* filter = reinterpret_cast<BrotliSourceStream*>(opaque);
    filter->FreeMemoryInternal(address);
  }

  void* AllocateMemoryInternal(size_t size) {
    size_t* array = reinterpret_cast<size_t*>(malloc(size + sizeof(size_t)));
    if (!array)
      return nullptr;
    used_memory_ += size;
    if (used_memory_maximum_ < used_memory_)
      used_memory_maximum_ = used_memory_;
    array[0] = size;
    return &array[1];
  }

  void FreeMemoryInternal(void* address) {
    if (!address)
      return;
    size_t* array = reinterpret_cast<size_t*>(address);
    used_memory_ -= array[-1];
    free(&array[-1]);
  }

  BrotliDecoderState* brotli_state_;

  DecodingStatus decoding_status_;

  size_t used_memory_;
  size_t used_memory_maximum_;
  size_t consumed_bytes_;
  size_t produced_bytes_;

  DISALLOW_COPY_AND_ASSIGN(BrotliSourceStream);
};

}  // namespace

std::unique_ptr<FilterSourceStream> CreateBrotliSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return std::make_unique<BrotliSourceStream>(std::move(previous));
}

}  // namespace net
