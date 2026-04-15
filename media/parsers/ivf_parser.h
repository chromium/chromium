// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_IVF_PARSER_H_
#define MEDIA_PARSERS_IVF_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace media {

const uint8_t kIvfHeaderSignature[] = {'D', 'K', 'I', 'F'};

#pragma pack(push, 1)
struct IvfFileHeader {
  char signature[4];        // signature: 'DKIF'
  uint16_t version;         // version (should be 0)
  uint16_t header_size;     // size of header in bytes
  uint32_t fourcc;          // codec FourCC (e.g., 'VP80')
  uint16_t width;           // width in pixels
  uint16_t height;          // height in pixels
  uint32_t timebase_denum;  // timebase denumerator
  uint32_t timebase_num;    // timebase numerator. For example, if
                            // timebase_denum is 30 and timebase_num is 2, the
                            // unit of IvfFrameHeader.timestamp is 2/30
                            // seconds.
  uint32_t num_frames;      // number of frames in file
  uint32_t unused;          // unused
};
static_assert(
    sizeof(IvfFileHeader) == 32,
    "sizeof(IvfFileHeader) must be fixed since it will be used with file IO");

struct IvfFrameHeader {
  uint32_t frame_size;  // Size of frame in bytes (not including the header)
  uint64_t timestamp;   // 64-bit presentation timestamp in unit timebase,
                        // which is defined in IvfFileHeader.
};
static_assert(
    sizeof(IvfFrameHeader) == 12,
    "sizeof(IvfFrameHeader) must be fixed since it will be used with file IO");
#pragma pack(pop)

// IVF is a simple container format for video frame. It is used by libvpx to
// transport VP8 and VP9 bitstream.
class IvfParser {
 public:
  IvfParser();

  IvfParser(const IvfParser&) = delete;
  IvfParser& operator=(const IvfParser&) = delete;

  // Initializes the parser for IVF |stream| and parses the
  // file header. Returns true on success.
  bool Initialize(base::span<const uint8_t> stream, IvfFileHeader* file_header);

  // Parses the next frame. If there is an error, returns empty span.
  // Otherwise, the returned span is the next frame.
  // |frame_header| will be filled with the frame header.
  // On success, it is guaranteed the span will be of size
  // |frame_header->frame_size|.
  base::span<const uint8_t> ParseNextFrame(IvfFrameHeader* frame_header);

 private:
  bool ParseFileHeader(IvfFileHeader* file_header);

  base::raw_span<const uint8_t> stream_;
};

}  // namespace media

#endif  // MEDIA_PARSERS_IVF_PARSER_H_
