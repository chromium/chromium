// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_
#define CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "net/filter/filter_source_stream.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace content {

// MerkleIntegritySourceStream decodes and validates content encoded with the
// "mi-sha256" content encoding
// (https://tools.ietf.org/html/draft-thomson-http-mice-03).
// TODO(ksakamoto): This class should eventually live in src/net/filter/.
class CONTENT_EXPORT MerkleIntegritySourceStream
    : public net::FilterSourceStream {
 public:
  MerkleIntegritySourceStream(std::string_view digest_header_value,
                              std::unique_ptr<SourceStream> upstream);

  MerkleIntegritySourceStream(const MerkleIntegritySourceStream&) = delete;
  MerkleIntegritySourceStream& operator=(const MerkleIntegritySourceStream&) =
      delete;

  ~MerkleIntegritySourceStream() override;

  // net::FilterSourceStream
  base::expected<size_t, net::Error> FilterData(
      net::IOBuffer* output_buffer,
      size_t output_buffer_size,
      net::IOBuffer* input_buffer,
      size_t input_buffer_size,
      size_t* consumed_bytes,
      bool upstream_eof_reached) override;
  std::string GetTypeAsString() const override;

 private:
  // Processes as many bytes of |input| as are available or fit in
  // |output|. Both |input| and |output| are advanced past any bytes consumed or
  // written to, respectively. Returns true if all input processed, possibly
  // none, was valid and false on fatal error.
  bool FilterDataImpl(base::span<char>* output,
                      base::span<const char>* input,
                      bool upstream_eof_reached);

  // Copies |partial_output_| to output, as much as fits and advances both
  // buffers. Returns whether all output was copied.
  bool CopyPartialOutput(base::span<char>* output);

  // Consumes the next |len| bytes of data from |partial_input_| and |input|
  // and, if available, points |result| to it and returns true. |result| will
  // point into either |input| or data copied to |storage|. |input| is advanced
  // past any consumed bytes. If |len| bytes are not available, returns false
  // and fully consumes |input| |partial_input_| for a future call.
  bool ConsumeBytes(base::span<const char>* input,
                    size_t len,
                    base::span<const char>* result,
                    std::string* storage);

  // Processes a record and returns whether it was valid. If valid, writes the
  // contents into |output|, advancing past any bytes written. If |output| was
  // not large enough, excess data will be copied into an internal buffer for a
  // future call.
  bool ProcessRecord(base::span<const char> record,
                     bool is_final,
                     base::span<char>* output);

  // The partial input block, if the previous input buffer was too small.
  std::string partial_input_;
  // The partial output block, if the previous output buffer was too small.
  std::string partial_output_;
  // The index of |partial_output_| that has not been returned yet.
  size_t partial_output_offset_ = 0;
  // SHA-256 hash for the next record, if |final_record_done_| is false.
  uint8_t next_proof_[SHA256_DIGEST_LENGTH];
  size_t record_size_ = 0;
  bool failed_ = false;
  // Whether the final record has been processed.
  bool final_record_done_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_
