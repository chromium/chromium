// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H266 Annex-B video stream parser,
// but it only handles NALU parsing.

#ifndef MEDIA_PARSERS_H266_NALU_PARSER_H_
#define MEDIA_PARSERS_H266_NALU_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

#include <vector>

#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/parsers/h264_bit_reader.h"
#include "media/parsers/h264_parser.h"

namespace media {

struct SubsampleEntry;

// For explanations of each struct and its members, see H.266 specification
// at http://www.itu.int/rec/T-REC-H.266.
struct MEDIA_EXPORT H266NALU {
  H266NALU();
  // NAL Unit types are taken from Table 5 of VVC/H.266 standard
  // http://www.itu.int/rec/T-REC-H.266-202204-I/en
  enum Type {
    // VCL types
    kTrail = 0,
    kSTSA = 1,
    kRADL = 2,
    kRASL = 3,
    kReservedVCL4 = 4,
    kReservedVCL5 = 5,
    kReservedVCL6 = 6,
    kIDRWithRADL = 7,
    kIDRNoLeadingPicture = 8,
    kCRA = 9,
    kGDR = 10,
    kReservedIRAP11 = 11,
    // Non-VCL types
    kOPI = 12,
    kDCI = 13,
    kVPS = 14,
    kSPS = 15,
    kPPS = 16,
    kPrefixAPS = 17,
    kSuffixAPS = 18,
    kPH = 19,
    kAUD = 20,
    kEOSequence = 21,
    kEOStream = 22,
    kPrefixSEI = 23,
    kSuffixSEI = 24,
    kFiller = 25,
    kReservedNonVCL26 = 26,
    kReservedNonVCL27 = 27,
    kReservedNonVCL28 = 28,
    kUnspecified29 = 29,
    kUnspecified30 = 30,
    kUnspecified31 = 31,
  };

  // After (without) start code; we don't own the underlying memory
  // and a shallow copy should be made when copying this struct.
  const uint8_t* data;
  off_t size;  // From after start code to start code of next NALU (or EOS).
  int nal_unit_type;
  int nuh_layer_id;
  int nuh_temporal_id_plus1;
};

// Class to parse an Annex-B H.266 stream NALUs.
class MEDIA_EXPORT H266NaluParser {
 public:
  enum Result {
    kOk,
    kInvalidStream,         // error in stream
    kUnsupportedStream,     // stream not supported by the parser
    kMissingParameterSet,   // missing PPS/SPS from what was parsed
    kMissingPictureHeader,  // missing picture header for current picture.
    kEndOfStream,           // end of stream
    kIgnored,               // current nalu should be ignored
  };

  H266NaluParser();

  H266NaluParser(const H266NaluParser&) = delete;
  H266NaluParser& operator=(const H266NaluParser&) = delete;

  virtual ~H266NaluParser();

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
  // parsing functions (ParseSPS, etc.) will parse this NALU.
  // If the caller wishes to skip the current NALU, it can call this function
  // again, instead of any NALU-type specific parse functions below.
  Result AdvanceToNextNALU(H266NALU* nalu);

  // The return value of this method changes for every successful call to
  // AdvanceToNextNALU().
  // This returns the subsample information for the last NALU that was output
  // from AdvanceToNextNALU().
  std::vector<SubsampleEntry> GetCurrentSubsamples();

 protected:
  H264BitReader br_;

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

  // Ranges of encrypted bytes in the buffer passed to SetEncryptedStream().
  Ranges<const uint8_t*> encrypted_ranges_;

  // This contains the range of the previous NALU found in
  // AdvanceToNextNalu(). Holds exactly one range.
  Ranges<const uint8_t*> previous_nalu_range_;
};

}  // namespace media
#endif  // MEDIA_PARSERS_H266_NALU_PARSER_H_
