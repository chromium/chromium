// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H265 Annex-B video stream parser.

#ifndef MEDIA_VIDEO_H265_PARSER_H_
#define MEDIA_VIDEO_H265_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/video/h264_bit_reader.h"
#include "media/video/h264_parser.h"

namespace media {

struct SubsampleEntry;

// For explanations of each struct and its members, see H.265 specification
// at http://www.itu.int/rec/T-REC-H.265.
struct MEDIA_EXPORT H265NALU {
  H265NALU();

  // NAL Unit types are taken from Table 7-1 of HEVC/H265 standard
  // http://www.itu.int/rec/T-REC-H.265-201410-I/en
  enum Type {
    TRAIL_N = 0,
    TRAIL_R = 1,
    TSA_N = 2,
    TSA_R = 3,
    STSA_N = 4,
    STSA_R = 5,
    RADL_N = 6,
    RADL_R = 7,
    RASL_N = 8,
    RASL_R = 9,
    RSV_VCL_N10 = 10,
    RSV_VCL_R11 = 11,
    RSV_VCL_N12 = 12,
    RSV_VCL_R13 = 13,
    RSV_VCL_N14 = 14,
    RSV_VCL_R15 = 15,
    BLA_W_LP = 16,
    BLA_W_RADL = 17,
    BLA_N_LP = 18,
    IDR_W_RADL = 19,
    IDR_N_LP = 20,
    CRA_NUT = 21,
    RSV_IRAP_VCL22 = 22,
    RSV_IRAP_VCL23 = 23,
    RSV_VCL24 = 24,
    RSV_VCL25 = 25,
    RSV_VCL26 = 26,
    RSV_VCL27 = 27,
    RSV_VCL28 = 28,
    RSV_VCL29 = 29,
    RSV_VCL30 = 30,
    RSV_VCL31 = 31,
    VPS_NUT = 32,
    SPS_NUT = 33,
    PPS_NUT = 34,
    AUD_NUT = 35,
    EOS_NUT = 36,
    EOB_NUT = 37,
    FD_NUT = 38,
    PREFIX_SEI_NUT = 39,
    SUFFIX_SEI_NUT = 40,
    RSV_NVCL41 = 41,
    RSV_NVCL42 = 42,
    RSV_NVCL43 = 43,
    RSV_NVCL44 = 44,
    RSV_NVCL45 = 45,
    RSV_NVCL46 = 46,
    RSV_NVCL47 = 47,
    UNSPEC48 = 48,
    UNSPEC49 = 49,
    UNSPEC50 = 50,
    UNSPEC51 = 51,
    UNSPEC52 = 52,
    UNSPEC53 = 53,
    UNSPEC54 = 54,
    UNSPEC55 = 55,
    UNSPEC56 = 56,
    UNSPEC57 = 57,
    UNSPEC58 = 58,
    UNSPEC59 = 59,
    UNSPEC60 = 60,
    UNSPEC61 = 61,
    UNSPEC62 = 62,
    UNSPEC63 = 63,
  };

  // After (without) start code; we don't own the underlying memory
  // and a shallow copy should be made when copying this struct.
  const uint8_t* data;
  off_t size;  // From after start code to start code of next NALU (or EOS).

  int nal_unit_type;
  int nuh_layer_id;
  int nuh_temporal_id_plus1;
};

// Class to parse an Annex-B H.265 stream.
class MEDIA_EXPORT H265Parser {
 public:
  enum Result {
    kOk,
    kInvalidStream,      // error in stream
    kUnsupportedStream,  // stream not supported by the parser
    kEOStream,           // end of stream
  };

  H265Parser();
  ~H265Parser();

  void Reset();
  // Set current stream pointer to |stream| of |stream_size| in bytes,
  // |stream| owned by caller.
  // |subsamples| contains information about what parts of |stream| are
  // encrypted.
  void SetStream(const uint8_t* stream, off_t stream_size);
  void SetEncryptedStream(const uint8_t* stream,
                          off_t stream_size,
                          const std::vector<SubsampleEntry>& subsamples);

  // Read the stream to find the next NALU, identify it and return
  // that information in |*nalu|. This advances the stream to the beginning
  // of this NALU, but not past it, so subsequent calls to NALU-specific
  // parsing functions (ParseSPS, etc.)  will parse this NALU.
  // If the caller wishes to skip the current NALU, it can call this function
  // again, instead of any NALU-type specific parse functions below.
  Result AdvanceToNextNALU(H265NALU* nalu);

 private:
  // Move the stream pointer to the beginning of the next NALU,
  // i.e. pointing at the next start code.
  // Return true if a NALU has been found.
  // If a NALU is found:
  // - its size in bytes is returned in |*nalu_size| and includes
  //   the start code as well as the trailing zero bits.
  // - the size in bytes of the start code is returned in |*start_code_size|.
  bool LocateNALU(off_t* nalu_size, off_t* start_code_size);

  // Pointer to the current NALU in the stream.
  const uint8_t* stream_;

  // Bytes left in the stream after the current NALU.
  off_t bytes_left_;

  H264BitReader br_;

  // Ranges of encrypted bytes in the buffer passed to
  // SetEncryptedStream().
  Ranges<const uint8_t*> encrypted_ranges_;

  DISALLOW_COPY_AND_ASSIGN(H265Parser);
};

}  // namespace media

#endif  // MEDIA_VIDEO_H265_PARSER_H_
