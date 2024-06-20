// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_JPEG_PARSER_H_
#define MEDIA_PARSERS_JPEG_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "media/base/media_export.h"

namespace media {

// It's not a full featured JPEG parser implementation. It only parses JPEG
// baseline sequential process (invalid or progressive JPEGs should fail but not
// crash). For explanations of each struct and its members, see JPEG
// specification at http://www.w3.org/Graphics/JPEG/itu-t81.pdf.

enum JpegMarker {
  JPEG_SOF0 = 0xC0,   // start of frame (baseline)
  JPEG_SOF1 = 0xC1,   // start of frame (extended sequential)
  JPEG_SOF2 = 0xC2,   // start of frame (progressive)
  JPEG_SOF3 = 0xC3,   // start of frame (lossless))
  JPEG_DHT = 0xC4,    // define huffman table
  JPEG_SOF5 = 0xC5,   // start of frame (differential, sequential)
  JPEG_SOF6 = 0xC6,   // start of frame (differential, progressive)
  JPEG_SOF7 = 0xC7,   // start of frame (differential, lossless)
  JPEG_SOF9 = 0xC9,   // start of frame (arithmetic coding, extended)
  JPEG_SOF10 = 0xCA,  // start of frame (arithmetic coding, progressive)
  JPEG_SOF11 = 0xCB,  // start of frame (arithmetic coding, lossless)
  JPEG_SOF13 = 0xCD,  // start of frame (differential, arithmetic, sequential)
  JPEG_SOF14 = 0xCE,  // start of frame (differential, arithmetic, progressive)
  JPEG_SOF15 = 0xCF,  // start of frame (differential, arithmetic, lossless)
  JPEG_RST0 = 0xD0,   // restart
  JPEG_RST1 = 0xD1,   // restart
  JPEG_RST2 = 0xD2,   // restart
  JPEG_RST3 = 0xD3,   // restart
  JPEG_RST4 = 0xD4,   // restart
  JPEG_RST5 = 0xD5,   // restart
  JPEG_RST6 = 0xD6,   // restart
  JPEG_RST7 = 0xD7,   // restart
  JPEG_SOI = 0xD8,    // start of image
  JPEG_EOI = 0xD9,    // end of image
  JPEG_SOS = 0xDA,    // start of scan
  JPEG_DQT = 0xDB,    // define quantization table
  JPEG_DRI = 0xDD,    // define restart internal
  JPEG_APP0 = 0xE0,   // start of application segment (APP0)
  JPEG_APP1 = 0xE1,   // start of application segment (APP1)
  JPEG_MARKER_PREFIX = 0xFF,  // jpeg marker prefix
};

// JPEG format uses 2 bytes to denote the size of a segment, and the size
// includes the 2 bytes used for specifying it. Therefore, maximum data size
// allowed is: 65535 - 2 = 65533.
constexpr size_t kMaxMarkerSizeAllowed = 65533;

// JPEG header only uses 2 bytes to represent width and height.
constexpr int kMaxDimension = 65535;

constexpr size_t kDctSize = 64;
constexpr size_t kNumDcRunSizeBits = 16;
constexpr size_t kNumAcRunSizeBits = 16;
constexpr size_t kNumDcCodeWordsHuffVal = 12;
constexpr size_t kNumAcCodeWordsHuffVal = 162;
constexpr size_t kJpegDefaultHeaderSize =
    67 + (kDctSize * 2) + (kNumDcRunSizeBits * 2) +
    (kNumDcCodeWordsHuffVal * 2) + (kNumAcRunSizeBits * 2) +
    (kNumAcCodeWordsHuffVal * 2);
constexpr size_t kJFIFApp0Size = 16;
constexpr size_t kJFIFApp1HeaderSize = 4;

const size_t kJpegMaxHuffmanTableNumBaseline = 2;
const size_t kJpegMaxComponents = 4;
const size_t kJpegMaxQuantizationTableNum = 4;

// Parsing result of JPEG DHT marker.
struct JpegHuffmanTable {
  bool valid;
  uint8_t code_length[16];
  uint8_t code_value[162];
};

// K.3.3.1 "Specification of typical tables for DC difference coding"
MEDIA_EXPORT
extern const JpegHuffmanTable kDefaultDcTable[kJpegMaxHuffmanTableNumBaseline];

// K.3.3.2 "Specification of typical tables for AC coefficient coding"
MEDIA_EXPORT
extern const JpegHuffmanTable kDefaultAcTable[kJpegMaxHuffmanTableNumBaseline];

// Parsing result of JPEG DQT marker.
struct JpegQuantizationTable {
  bool valid;
  uint8_t value[kDctSize];  // baseline only supports 8 bits quantization table
};

MEDIA_EXPORT extern const uint8_t kZigZag8x8[64];

// Table K.1 Luminance quantization table
// Table K.2 Chrominance quantization table
MEDIA_EXPORT
extern const JpegQuantizationTable kDefaultQuantTable[2];

// Parsing result of a JPEG component.
struct JpegComponent {
  uint8_t id;
  uint8_t horizontal_sampling_factor;
  uint8_t vertical_sampling_factor;
  uint8_t quantization_table_selector;
};

// Parsing result of a JPEG SOF marker.
struct JpegFrameHeader {
  uint16_t visible_width;
  uint16_t visible_height;
  uint16_t coded_width;
  uint16_t coded_height;
  uint8_t num_components;
  JpegComponent components[kJpegMaxComponents];
};

// Parsing result of JPEG SOS marker.
struct JpegScanHeader {
  uint8_t num_components;
  struct Component {
    uint8_t component_selector;
    uint8_t dc_selector;
    uint8_t ac_selector;
  } components[kJpegMaxComponents];
};

struct JpegParseResult {
  JpegFrameHeader frame_header;
  JpegHuffmanTable dc_table[kJpegMaxHuffmanTableNumBaseline];
  JpegHuffmanTable ac_table[kJpegMaxHuffmanTableNumBaseline];
  JpegQuantizationTable q_table[kJpegMaxQuantizationTableNum];
  uint16_t restart_interval;
  JpegScanHeader scan;
  const char* data;
  // The size of compressed data of the first image.
  size_t data_size;
  // The size of the first entire image including header.
  size_t image_size;
};

// Parses JPEG picture in |buffer| with |length|.  Returns true iff header is
// valid and JPEG baseline sequential process is present. If parsed
// successfully, |result| is the parsed result.
MEDIA_EXPORT
bool ParseJpegPicture(base::span<const uint8_t> buffer,
                      JpegParseResult* result);

// Parses the first image of JPEG stream in |buffer| with |length|.  Returns
// true iff header is valid and JPEG baseline sequential process is present.
// If parsed successfully, |result| is the parsed result.
MEDIA_EXPORT
bool ParseJpegStream(base::span<const uint8_t> buffer, JpegParseResult* result);

}  // namespace media

#endif  // MEDIA_PARSERS_JPEG_PARSER_H_
