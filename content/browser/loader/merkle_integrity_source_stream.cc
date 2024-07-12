// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/loader/merkle_integrity_source_stream.h"

#include <string.h>

#include <string_view>

#include "base/base64.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"

namespace content {

namespace {

// Limit the record size to 16KiB to prevent browser OOM. This matches the
// maximum record size in TLS and the default maximum frame size in HTTP/2.
constexpr uint64_t kMaxRecordSize = 16 * 1024;

constexpr char kMiSha256Header[] = "mi-sha256-03=";
constexpr size_t kMiSha256HeaderLength = sizeof(kMiSha256Header) - 1;

// Copies as many bytes from |input| as will fit in |output| and advances both.
size_t CopyClamped(base::span<const char>* input, base::span<char>* output) {
  size_t size = std::min(output->size(), input->size());
  base::ranges::copy(input->first(size), output->data());
  *output = output->subspan(size);
  *input = input->subspan(size);
  return size;
}

}  // namespace

MerkleIntegritySourceStream::MerkleIntegritySourceStream(
    std::string_view digest_header_value,
    std::unique_ptr<SourceStream> upstream)
    // TODO(ksakamoto): Use appropriate SourceType.
    : net::FilterSourceStream(SourceStream::TYPE_NONE, std::move(upstream)) {
  std::string next_proof;
  if (!base::StartsWith(digest_header_value, kMiSha256Header) ||
      !base::Base64Decode(digest_header_value.substr(kMiSha256HeaderLength),
                          &next_proof) ||
      next_proof.size() != SHA256_DIGEST_LENGTH) {
    failed_ = true;
  } else {
    memcpy(next_proof_, next_proof.data(), SHA256_DIGEST_LENGTH);
  }
}

MerkleIntegritySourceStream::~MerkleIntegritySourceStream() = default;

base::expected<size_t, net::Error> MerkleIntegritySourceStream::FilterData(
    net::IOBuffer* output_buffer,
    size_t output_buffer_size,
    net::IOBuffer* input_buffer,
    size_t input_buffer_size,
    size_t* consumed_bytes,
    bool upstream_eof_reached) {
  if (failed_) {
    return base::unexpected(net::ERR_CONTENT_DECODING_FAILED);
  }

  base::span<const char> remaining_input =
      base::make_span(input_buffer->data(), input_buffer_size);
  base::span<char> remaining_output =
      base::make_span(output_buffer->data(), output_buffer_size);
  bool ok =
      FilterDataImpl(&remaining_output, &remaining_input, upstream_eof_reached);
  *consumed_bytes = input_buffer_size - remaining_input.size();
  if (!ok) {
    failed_ = true;
    return base::unexpected(net::ERR_CONTENT_DECODING_FAILED);
  }
  return output_buffer_size - remaining_output.size();
}

std::string MerkleIntegritySourceStream::GetTypeAsString() const {
  return "MI-SHA256";
}

bool MerkleIntegritySourceStream::FilterDataImpl(base::span<char>* output,
                                                 base::span<const char>* input,
                                                 bool upstream_eof_reached) {
  std::string storage;

  // Process the record size in front, if we haven't yet.
  if (record_size_ == 0) {
    base::span<const char> bytes;
    if (!ConsumeBytes(input, 8, &bytes, &storage)) {
      if (!upstream_eof_reached) {
        return true;  // Wait for more data later.
      }
      if (partial_input_.empty()) {
        // As a special case, the encoding of an empty payload is itself an
        // empty message (i.e. it omits the initial record size), and its
        // integrity proof is SHA-256("\0").
        final_record_done_ = true;
        return ProcessRecord({}, final_record_done_, output);
      }
      return false;
    }
    uint64_t record_size =
        base::U64FromBigEndian(base::as_bytes(bytes).first<8u>());
    if (record_size == 0u) {
      return false;
    }
    if (record_size > kMaxRecordSize) {
      DVLOG(1)
          << "Rejecting MI content encoding because record size is too big: "
          << record_size;
      return false;
    }
    record_size_ = base::checked_cast<size_t>(record_size);
  }

  // Clear any previous output before continuing.
  if (!CopyPartialOutput(output)) {
    DCHECK(output->empty());
    return true;
  }

  // Process records until we're done or there's no more room in |output|.
  while (!output->empty() && !final_record_done_) {
    base::span<const char> record;
    if (!ConsumeBytes(input, record_size_ + SHA256_DIGEST_LENGTH, &record,
                      &storage)) {
      DCHECK(input->empty());
      if (!upstream_eof_reached) {
        return true;  // Wait for more data later.
      }

      // The final record is shorter and does not contain a hash. Process all
      // remaining input as the final record.
      if (partial_input_.empty() || partial_input_.size() > record_size_) {
        return false;
      }
      record = partial_input_;
      final_record_done_ = true;
    }
    if (!ProcessRecord(record, final_record_done_, output)) {
      return false;
    }
  }

  if (final_record_done_) {
    DCHECK(upstream_eof_reached);
    DCHECK(input->empty());
  }
  return true;
}

bool MerkleIntegritySourceStream::CopyPartialOutput(base::span<char>* output) {
  if (partial_output_offset_ == partial_output_.size()) {
    return true;
  }
  base::span<const char> partial =
      base::make_span(partial_output_).subspan(partial_output_offset_);
  partial_output_offset_ += CopyClamped(&partial, output);
  if (partial_output_offset_ < partial_output_.size()) {
    return false;
  }
  partial_output_.clear();
  partial_output_offset_ = 0;
  return true;
}

bool MerkleIntegritySourceStream::ConsumeBytes(base::span<const char>* input,
                                               size_t len,
                                               base::span<const char>* result,
                                               std::string* storage) {
  // This comes from the requirement that, when ConsumeBytes returns false, the
  // next call must use the same |len|.
  DCHECK_LT(partial_input_.size(), len);

  // Return data directly from |input| if possible.
  if (partial_input_.empty() && input->size() >= len) {
    *result = input->subspan(0, len);
    *input = input->subspan(len);
    return true;
  }

  // Reassemble |len| bytes from |partial_input_| and |input|.
  size_t to_copy = std::min(len - partial_input_.size(), input->size());
  partial_input_.append(input->data(), to_copy);
  *input = input->subspan(to_copy);

  if (partial_input_.size() < len) {
    return false;
  }
  *storage = std::move(partial_input_);
  partial_input_.clear();
  *result = *storage;
  return true;
}

bool MerkleIntegritySourceStream::ProcessRecord(base::span<const char> record,
                                                bool is_final,
                                                base::span<char>* output) {
  DCHECK(partial_output_.empty());

  // Check the hash.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, reinterpret_cast<const uint8_t*>(record.data()),
                record.size());
  uint8_t type = is_final ? 0 : 1;
  SHA256_Update(&ctx, &type, 1);
  uint8_t sha256[SHA256_DIGEST_LENGTH];
  SHA256_Final(sha256, &ctx);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // The fuzzer will have a hard time fixing up chains of hashes, so, if
  // building in fuzzer mode, everything hashes to the same garbage value.
  memset(sha256, 0x42, SHA256_DIGEST_LENGTH);
#endif
  if (memcmp(sha256, next_proof_, SHA256_DIGEST_LENGTH) != 0) {
    return false;
  }

  if (!is_final) {
    // Split into data and a hash.
    base::span<const char> hash = record.subspan(record_size_);
    record = record.first(record_size_);

    // Save the next proof.
    CHECK_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), hash.size());
    memcpy(next_proof_, hash.data(), SHA256_DIGEST_LENGTH);
  }

  // Copy whatever output there is room for.
  CopyClamped(&record, output);

  // If it didn't all fit, save the remaining in |partial_output_|.
  DCHECK(record.empty() || output->empty());
  partial_output_.append(record.data(), record.size());
  return true;
}

}  // namespace content
