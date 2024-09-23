// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a H26xAnnexBBitstreamBuilder class
// for constructing raw bitstream buffers containing NAL units in H.264 Annex-B
// stream format.
// See H.264 spec Annex B and chapter 7 for more details.

#ifndef MEDIA_FILTERS_H26X_ANNEX_B_BITSTREAM_BUILDER_H_
#define MEDIA_FILTERS_H26X_ANNEX_B_BITSTREAM_BUILDER_H_

#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/h265_nalu_parser.h"

namespace media {

// Holds one or more NALUs as a raw bitstream buffer in H.264 Annex-B format.
// Note that this class currently does NOT insert emulation prevention
// three-byte sequences (spec 7.3.1) by default.
class MEDIA_EXPORT H26xAnnexBBitstreamBuilder {
 public:
  // This is used by VA-API encoder and D3D12 encoder.
  // - For VA-API encoder, set |insert_emulation_prevention_bytes| to |false| as
  //   VA-API takes SPS/PPS RBSP and outputs the AnnexB bitstream.
  // - For D3D12 encoder, set |insert_emulation_prevention_bytes| to |true| as
  //   it only outputs slice NALU. We add SPS/PPS with EPB in Chromium to create
  //   an AnnexB bitstream.
  explicit H26xAnnexBBitstreamBuilder(
      bool insert_emulation_prevention_bytes = false);
  ~H26xAnnexBBitstreamBuilder();
  H26xAnnexBBitstreamBuilder(const H26xAnnexBBitstreamBuilder&) = delete;
  H26xAnnexBBitstreamBuilder& operator=(const H26xAnnexBBitstreamBuilder&) =
      delete;

  // Discard all data and reset the buffer for reuse.
  void Reset();

  // Append |num_bits| bits to the stream from |val|.
  // |val| is interpreted in the host endianness.
  template <typename T>
  void AppendBits(size_t num_bits, T val) {
    AppendU64(num_bits, static_cast<uint64_t>(val));
  }

  void AppendBits(size_t num_bits, bool val) {
    DCHECK_EQ(num_bits, 1ul);
    AppendBool(val);
  }

  // Append a one-bit bool/flag value |val| to the stream.
  void AppendBool(bool val);

  // Append a signed value in |val| in Exp-Golomb code.
  void AppendSE(int val);

  // Append an unsigned value in |val| in Exp-Golomb code.
  void AppendUE(unsigned int val);

  // Start a new NALU of type |nalu_type| and with given |nal_ref_idc|
  // (see spec). Note, that until FinishNALU() is called, some of the bits
  // may not be flushed into the buffer and the data will not be correctly
  // aligned with trailing bits.
  void BeginNALU(H264NALU::Type nalu_type, int nal_ref_idc);

  // Start a H265 NALU.
  void BeginNALU(H265NALU::Type nalu_type);

  // Finish current NALU. This will flush any cached bits and correctly align
  // the buffer with RBSP trailing bits. This MUST be called for the stream
  // returned by data() to be correct.
  void FinishNALU();

  // Finishes current bit stream. This will flush any cached bits in the reg
  // without RBSP trailing bits alignment. e.g. for packed slice header, it is
  // not a complete NALU, the slice data and RBSP trailing will be filled by
  // user mode driver. This MUST be called for the stream returned by data() to
  // be correct.
  void Flush();

  // Return number of full bytes in the stream. Note that FinishNALU() has to
  // be called to flush cached bits, or the return value will not include them.
  size_t BytesInBuffer() const;

  // Returns number of bits in the stream. Note that FinishNALU() or Flush() has
  // to be called to flush cached bits, or the return value will not include
  // them.
  size_t BitsInBuffer() const;

  // Return a pointer to the stream. FinishNALU() must be called before
  // accessing the stream, otherwise some bits may still be cached and not in
  // the buffer.
  //
  // TODO(crbug.com/40284755): Return a span up to `pos_` which is the range of
  // initialized bytes in `data_`.
  const uint8_t* data() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(H26xAnnexBBitstreamBuilderAppendBitsTest,
                           AppendAndVerifyBits);

  // Allocate additional memory (kGrowBytes bytes) for the buffer.
  void Grow();

  // Append |num_bits| bits from U64 value |val| (in host endianness).
  void AppendU64(size_t num_bits, uint64_t val);

  // Flush any cached bits in the reg with byte granularity, i.e. enough
  // bytes to flush all pending bits, but not more.
  void FlushReg();

  typedef uint64_t RegType;
  enum {
    // Sizes of reg_.
    kRegByteSize = sizeof(RegType),
    kRegBitSize = kRegByteSize * 8,
    // Amount of bytes to grow the buffer by when we run out of
    // previously-allocated memory for it.
    kGrowBytes = 4096,
  };

  static_assert(kGrowBytes >= kRegByteSize,
                "kGrowBytes must be larger than kRegByteSize");

  // Whether to insert emulation prevention bytes in RBSP.
  bool insert_emulation_prevention_bytes_;

  // Whether BeginNALU() has been called but not FinishNALU().
  bool in_nalu_;

  // Unused bits left in reg_.
  size_t bits_left_in_reg_;

  // Cache for appended bits. Bits are flushed to data_ with kRegByteSize
  // granularity, i.e. when reg_ becomes full, or when an explicit FlushReg()
  // is called.
  RegType reg_;

  // Current byte offset in data_ (points to the start of unwritten bits).
  size_t pos_;
  // Current last bit in data_ (points to the start of unwritten bit).
  size_t bits_in_buffer_;

  // Buffer for stream data. Only the bytes before `pos_` have been initialized.
  base::HeapArray<uint8_t> data_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_H26X_ANNEX_B_BITSTREAM_BUILDER_H_
