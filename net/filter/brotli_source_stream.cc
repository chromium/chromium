// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/filter/source_stream_type.h"
#include "net/net_buildflags.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "third_party/brotli/include/brotli/shared_dictionary.h"

#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
#include "net/filter/rust_brotli_decoder.h"
#endif

namespace net {

namespace {

const char kBrotli[] = "BROTLI";

// The minimum decompressed size of a body for
// Net.BrotliFilter.DecodingThroughput, so that its decoding time is long enough
// to measure precisely.
constexpr size_t kMinBytesForDecodingThroughput = 32 * 1024;

// The names of a histogram for all bodies and of its variants for the content
// encoding of the body: "br", or "dcb" with a shared dictionary.
struct HistogramNames {
  std::string_view all;
  std::string_view br;
  std::string_view dcb;
};

// LINT.IfChange(BrotliFilterEncoding)
constexpr HistogramNames kStatusHistograms = {
    "BrotliFilter.Status", "BrotliFilter.Status.Br", "BrotliFilter.Status.Dcb"};
constexpr HistogramNames kErrorCodeHistograms = {"BrotliFilter.ErrorCode",
                                                 "BrotliFilter.ErrorCode.Br",
                                                 "BrotliFilter.ErrorCode.Dcb"};
constexpr HistogramNames kDecodingTimeHistograms = {
    "Net.BrotliFilter.DecodingTime", "Net.BrotliFilter.DecodingTime.Br",
    "Net.BrotliFilter.DecodingTime.Dcb"};
constexpr HistogramNames kDecodingThroughputHistograms = {
    "Net.BrotliFilter.DecodingThroughput",
    "Net.BrotliFilter.DecodingThroughput.Br",
    "Net.BrotliFilter.DecodingThroughput.Dcb"};
// LINT.ThenChange(//tools/metrics/histograms/variants.xml:BrotliFilterEncoding)

#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
// The values of `rust_brotli::DecodeResult::result` (see
// //net/filter/rust_brotli_decoder.rs).
static_assert(BROTLI_DECODER_RESULT_ERROR == 0);
static_assert(BROTLI_DECODER_RESULT_SUCCESS == 1);
static_assert(BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT == 2);
static_assert(BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT == 3);
#endif

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
        dictionary_size_(dictionary_size) {
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
    if (base::FeatureList::IsEnabled(features::kRustBrotliInNet)) {
      rust_decoder_ = std::make_unique<rust_brotli::Decoder>();
      if (dictionary_) {
        CHECK(rust_decoder_->attach_dictionary(dictionary_size_));
      }
      return;
    }
#endif
    // The nullptrs mean the decoder will use malloc() and free() directly.
    brotli_state_.reset(BrotliDecoderCreateInstance(nullptr, nullptr, nullptr));
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
    // Streams that didn't get any input, for example because the request was
    // canceled, would skew the histograms.
    if (consumed_bytes_ == 0 &&
        decoding_status_ == DecodingStatus::DECODING_IN_PROGRESS) {
      return;
    }

    RecordWithEncodingVariant(kStatusHistograms, [&](std::string_view name) {
      base::UmaHistogramEnumeration(name, decoding_status_);
    });
    // Only errors have negative codes.
    const int error_code = GetErrorCode();
    if (error_code < 0) {
      RecordWithEncodingVariant(
          kErrorCodeHistograms, [&](std::string_view name) {
            base::UmaHistogramExactLinear(name, -error_code,
                                          1 - BROTLI_LAST_ERROR_CODE);
          });
    }
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      // CompressionPercent is undefined when there is no output produced.
      if (produced_bytes_ != 0) {
        UMA_HISTOGRAM_PERCENTAGE(
            "BrotliFilter.CompressionPercent",
            static_cast<int>((consumed_bytes_ * 100) / produced_bytes_));
      }
      RecordWithEncodingVariant(
          kDecodingTimeHistograms, [&](std::string_view name) {
            base::UmaHistogramMicrosecondsTimes(name, decoding_time_);
          });
      // Like the microsecond histogram above, the throughput needs a
      // high-resolution clock.
      if (base::TimeTicks::IsHighResolution() &&
          produced_bytes_ >= kMinBytesForDecodingThroughput &&
          decoding_time_.is_positive()) {
        // Bytes per microsecond are KB (1000 bytes) per millisecond.
        const int kb_per_ms = base::ClampRound(
            produced_bytes_ / decoding_time_.InMicrosecondsF());
        RecordWithEncodingVariant(
            kDecodingThroughputHistograms, [&](std::string_view name) {
              base::UmaHistogramCustomCounts(name, kb_per_ms, 1, 10000, 100);
            });
      }
    }
  }

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(DecodingStatus)
  enum class DecodingStatus : int {
    DECODING_IN_PROGRESS = 0,
    DECODING_DONE = 1,
    DECODING_ERROR = 2,
    kMaxValue = DECODING_ERROR,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:BrotliFilterDecodingStatus)

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

    base::span<const uint8_t> input = input_buffer->first(input_buffer_size);
    base::span<uint8_t> output = output_buffer->first(output_buffer_size);

    const base::ElapsedTimer decoding_timer;
    const BrotliDecoderResult result = Decompress(input, output);
    decoding_time_ += decoding_timer.Elapsed();

    size_t bytes_used = input_buffer_size - input.size();
    size_t bytes_written = output_buffer_size - output.size();
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

  // Decodes as much of `input` into `output` as possible, with the decoder of
  // this stream, and removes the bytes that it read and wrote from them.
  BrotliDecoderResult Decompress(base::span<const uint8_t>& input,
                                 base::span<uint8_t>& output) {
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
    if (rust_decoder_) {
      const base::span<const uint8_t> dictionary =
          dictionary_ ? dictionary_->first(dictionary_size_)
                      : base::span<const uint8_t>();
      const rust_brotli::DecodeResult result =
          rust_decoder_->decompress_stream(dictionary, input, output);
      input = input.subspan(result.bytes_read);
      output = output.subspan(result.bytes_written);
      return static_cast<BrotliDecoderResult>(result.result);
    }
#endif
    const uint8_t* next_in = input.data();
    size_t available_in = input.size();
    uint8_t* next_out = output.data();
    size_t available_out = output.size();
    const BrotliDecoderResult result = BrotliDecoderDecompressStream(
        brotli_state_.get(), &available_in, &next_in, &available_out, &next_out,
        /*total_out=*/nullptr);
    input = input.last(available_in);
    output = output.last(available_out);
    return result;
  }

  // Returns the error that the decoder found, a `BrotliDecoderErrorCode`, or a
  // value that isn't negative if there is none. It's an `int`, because codes
  // that the Rust crate may add wouldn't be valid `BrotliDecoderErrorCode`s.
  int GetErrorCode() const {
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
    if (rust_decoder_) {
      return rust_decoder_->error_code();
    }
#endif
    return BrotliDecoderGetErrorCode(brotli_state_.get());
  }

  // Records a sample with `record` in the histogram for all bodies and in its
  // variant for the content encoding of this body.
  template <typename RecordFunction>
  void RecordWithEncodingVariant(const HistogramNames& histograms,
                                 RecordFunction record) const {
    record(histograms.all);
    record(dictionary_ ? histograms.dcb : histograms.br);
  }

  const scoped_refptr<IOBuffer> dictionary_;
  const size_t dictionary_size_;

  // Only one of these is set, depending on `features::kRustBrotliInNet`. The C
  // decoder keeps a pointer to `dictionary_`, so it must be destroyed first.
  std::unique_ptr<BrotliDecoderState, BrotliDecoderStateDeleter> brotli_state_;
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
  std::unique_ptr<rust_brotli::Decoder> rust_decoder_;
#endif

  DecodingStatus decoding_status_ = DecodingStatus::DECODING_IN_PROGRESS;

  size_t consumed_bytes_ = 0;
  size_t produced_bytes_ = 0;

  // The time spent in the decoder. It doesn't include the time waiting for
  // input.
  base::TimeDelta decoding_time_;
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
