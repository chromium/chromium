// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/jpeg_parser.h"

#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"

#define READ_U8_OR_RETURN_FALSE(out)                                       \
  do {                                                                     \
    uint8_t _out;                                                          \
    if (!reader.ReadU8BigEndian(_out)) {                                   \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return false;                                                        \
    }                                                                      \
    out = _out;                                                            \
  } while (0)

#define READ_U16_OR_RETURN_FALSE(out)                                      \
  do {                                                                     \
    uint16_t _out;                                                         \
    if (!reader.ReadU16BigEndian(_out)) {                                  \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return false;                                                        \
    }                                                                      \
    out = _out;                                                            \
  } while (0)

namespace media {

const JpegHuffmanTable kDefaultDcTable[kJpegMaxHuffmanTableNumBaseline] = {
    // luminance DC coefficients
    {
        true,
        {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
         0x0b},
    },
    // chrominance DC coefficients
    {
        true,
        {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb},
    },
};

const JpegHuffmanTable kDefaultAcTable[kJpegMaxHuffmanTableNumBaseline] = {
    // luminance AC coefficients
    {
        true,
        {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d},
        {0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
         0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
         0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
         0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
         0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
         0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
         0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
         0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
         0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
         0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
         0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
         0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
         0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4,
         0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa},
    },
    // chrominance AC coefficients
    {
        true,
        {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77},
        {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41,
         0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
         0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1,
         0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
         0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
         0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
         0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
         0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
         0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
         0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
         0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
         0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
         0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
         0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa},
    },
};

constexpr uint8_t kZigZag8x8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

constexpr JpegQuantizationTable kDefaultQuantTable[2] = {
    // Table K.1 Luminance quantization table values.
    {
        true,
        {16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
         14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
         18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
         49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99},
    },
    // Table K.2 Chrominance quantization table values.
    {
        true,
        {17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
         24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
         99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
         99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99},
    },
};

static bool InRange(int value, int a, int b) {
  return a <= value && value <= b;
}

// Round up |value| to multiple of |mul|. |value| must be non-negative.
// |mul| must be positive.
static int RoundUp(int value, int mul) {
  DCHECK_GE(value, 0);
  DCHECK_GE(mul, 1);
  return (value + mul - 1) / mul * mul;
}

// |frame_header| is already initialized to 0 in ParseJpegPicture.
static bool ParseSOF(base::span<const uint8_t> buffer,
                     JpegFrameHeader* frame_header) {
  // Spec B.2.2 Frame header syntax
  DCHECK(frame_header);
  auto reader = base::SpanReader(buffer);

  uint8_t precision;
  READ_U8_OR_RETURN_FALSE(precision);
  READ_U16_OR_RETURN_FALSE(frame_header->visible_height);
  READ_U16_OR_RETURN_FALSE(frame_header->visible_width);
  READ_U8_OR_RETURN_FALSE(frame_header->num_components);

  if (precision != 8) {
    DLOG(ERROR) << "Only support 8-bit precision, not "
                << static_cast<int>(precision) << " bit for baseline";
    return false;
  }
  if (!InRange(frame_header->num_components, 1,
               std::size(frame_header->components))) {
    DLOG(ERROR) << "num_components="
                << static_cast<int>(frame_header->num_components)
                << " is not supported";
    return false;
  }

  int max_h_factor = 0;
  int max_v_factor = 0;
  for (size_t i = 0; i < frame_header->num_components; i++) {
    JpegComponent& component = frame_header->components[i];
    READ_U8_OR_RETURN_FALSE(component.id);
    if (component.id > frame_header->num_components) {
      DLOG(ERROR) << "component id (" << static_cast<int>(component.id)
                  << ") should be <= num_components ("
                  << static_cast<int>(frame_header->num_components) << ")";
      return false;
    }
    uint8_t hv;
    READ_U8_OR_RETURN_FALSE(hv);
    component.horizontal_sampling_factor = hv / 16;
    component.vertical_sampling_factor = hv % 16;
    if (component.horizontal_sampling_factor > max_h_factor)
      max_h_factor = component.horizontal_sampling_factor;
    if (component.vertical_sampling_factor > max_v_factor)
      max_v_factor = component.vertical_sampling_factor;
    if (!InRange(component.horizontal_sampling_factor, 1, 4)) {
      DVLOG(1) << "Invalid horizontal sampling factor "
               << static_cast<int>(component.horizontal_sampling_factor);
      return false;
    }
    if (!InRange(component.vertical_sampling_factor, 1, 4)) {
      DVLOG(1) << "Invalid vertical sampling factor "
               << static_cast<int>(component.horizontal_sampling_factor);
      return false;
    }
    READ_U8_OR_RETURN_FALSE(component.quantization_table_selector);
  }

  // The size of data unit is 8*8 and the coded size should be extended
  // to complete minimum coded unit, MCU. See Spec A.2.
  frame_header->coded_width =
      RoundUp(frame_header->visible_width, max_h_factor * 8);
  frame_header->coded_height =
      RoundUp(frame_header->visible_height, max_v_factor * 8);

  return true;
}

// |q_table| is already initialized to 0 in ParseJpegPicture.
static bool ParseDQT(base::span<const uint8_t> buffer,
                     JpegQuantizationTable* q_table) {
  // Spec B.2.4.1 Quantization table-specification syntax
  DCHECK(q_table);
  auto reader = base::SpanReader(buffer);
  while (reader.remaining() > 0u) {
    uint8_t precision_and_table_id;
    READ_U8_OR_RETURN_FALSE(precision_and_table_id);
    uint8_t precision = precision_and_table_id / 16;
    uint8_t table_id = precision_and_table_id % 16;
    if (!InRange(precision, 0, 1)) {
      DVLOG(1) << "Invalid precision " << static_cast<int>(precision);
      return false;
    }
    if (precision == 1) {  // 1 means 16-bit precision
      DLOG(ERROR) << "An 8-bit DCT-based process shall not use a 16-bit "
                  << "precision quantization table";
      return false;
    }
    if (table_id >= kJpegMaxQuantizationTableNum) {
      DLOG(ERROR) << "Quantization table id (" << static_cast<int>(table_id)
                  << ") exceeded " << kJpegMaxQuantizationTableNum;
      return false;
    }

    if (!reader.ReadCopy(q_table[table_id].value)) {
      return false;
    }
    q_table[table_id].valid = true;
  }
  return true;
}

// |dc_table| and |ac_table| are already initialized to 0 in ParseJpegPicture.
static bool ParseDHT(base::span<const uint8_t> buffer,
                     JpegHuffmanTable* dc_table,
                     JpegHuffmanTable* ac_table) {
  // Spec B.2.4.2 Huffman table-specification syntax
  DCHECK(dc_table);
  DCHECK(ac_table);
  auto reader = base::SpanReader(buffer);
  while (reader.remaining() > 0u) {
    uint8_t table_class_and_id;
    READ_U8_OR_RETURN_FALSE(table_class_and_id);
    int table_class = table_class_and_id / 16;
    int table_id = table_class_and_id % 16;
    if (!InRange(table_class, 0, 1)) {
      DVLOG(1) << "Invalid table class " << table_class;
      return false;
    }
    if (table_id >= 2) {
      DLOG(ERROR) << "Table id(" << table_id
                  << ") >= 2 is invalid for baseline profile";
      return false;
    }

    JpegHuffmanTable* table;
    if (table_class == 1)
      table = &ac_table[table_id];
    else
      table = &dc_table[table_id];

    size_t count = 0u;
    if (!reader.ReadCopy(table->code_length)) {
      return false;
    }
    for (size_t i = 0; i < std::size(table->code_length); i++)
      count += table->code_length[i];

    if (!InRange(count, 0u, sizeof(table->code_value))) {
      DVLOG(1) << "Invalid code count " << count;
      return false;
    }
    if (!reader.ReadCopy(base::span(table->code_value).first(count))) {
      return false;
    }
    table->valid = true;
  }
  return true;
}

static bool ParseDRI(base::span<const uint8_t> buffer,
                     uint16_t* restart_interval) {
  // Spec B.2.4.4 Restart interval definition syntax
  DCHECK(restart_interval);
  if (buffer.size() != 2) {
    return false;
  }
  *restart_interval = base::U16FromBigEndian(buffer.first<2>());
  return true;
}

// |scan| is already initialized to 0 in ParseJpegPicture.
static bool ParseSOS(base::span<const uint8_t> buffer,
                     const JpegFrameHeader& frame_header,
                     JpegScanHeader* scan) {
  // Spec B.2.3 Scan header syntax
  DCHECK(scan);
  auto reader = base::SpanReader(buffer);
  READ_U8_OR_RETURN_FALSE(scan->num_components);
  if (scan->num_components != frame_header.num_components) {
    DLOG(ERROR) << "The number of scan components ("
                << static_cast<int>(scan->num_components)
                << ") mismatches the number of image components ("
                << static_cast<int>(frame_header.num_components) << ")";
    return false;
  }

  for (int i = 0; i < scan->num_components; i++) {
    JpegScanHeader::Component* component = &scan->components[i];
    READ_U8_OR_RETURN_FALSE(component->component_selector);
    uint8_t dc_and_ac_selector;
    READ_U8_OR_RETURN_FALSE(dc_and_ac_selector);
    component->dc_selector = dc_and_ac_selector / 16;
    component->ac_selector = dc_and_ac_selector % 16;
    if (component->component_selector != frame_header.components[i].id) {
      DLOG(ERROR) << "component selector mismatches image component id";
      return false;
    }
    if (component->dc_selector >= kJpegMaxHuffmanTableNumBaseline) {
      DLOG(ERROR) << "DC selector (" << static_cast<int>(component->dc_selector)
                  << ") should be 0 or 1 for baseline mode";
      return false;
    }
    if (component->ac_selector >= kJpegMaxHuffmanTableNumBaseline) {
      DLOG(ERROR) << "AC selector (" << static_cast<int>(component->ac_selector)
                  << ") should be 0 or 1 for baseline mode";
      return false;
    }
  }

  // Unused fields, only for value checking.
  uint8_t spectral_selection_start;
  uint8_t spectral_selection_end;
  uint8_t point_transform;
  READ_U8_OR_RETURN_FALSE(spectral_selection_start);
  READ_U8_OR_RETURN_FALSE(spectral_selection_end);
  READ_U8_OR_RETURN_FALSE(point_transform);
  if (spectral_selection_start != 0 || spectral_selection_end != 63) {
    DLOG(ERROR) << "Spectral selection should be 0,63 for baseline mode";
    return false;
  }
  if (point_transform != 0) {
    DLOG(ERROR) << "Point transform should be 0 for baseline mode";
    return false;
  }

  return true;
}

// |eoi_begin_ptr| will point to the beginning of the EOI marker (the FF byte)
// and |eoi_end_ptr| will point to the end of image (right after the end of the
// EOI marker) after search succeeds. Returns true on EOI marker found, or false
// otherwise.
static bool SearchEOI(base::span<const uint8_t> buffer,
                      const char** eoi_begin_ptr,
                      const char** eoi_end_ptr) {
  DCHECK(eoi_begin_ptr);
  DCHECK(eoi_end_ptr);
  auto reader = base::SpanReader(buffer);
  uint8_t marker2;

  while (reader.remaining() > 0u) {
    size_t marker1_in_buffer;
    {
      auto search_span = reader.remaining_span();
      auto it = std::ranges::find(search_span, JPEG_MARKER_PREFIX);
      if (it == search_span.end()) {
        return false;
      }
      size_t found_offset = it - search_span.begin();
      marker1_in_buffer = reader.num_read() + found_offset;
      reader.Skip(found_offset + 1u);
    }

    do {
      READ_U8_OR_RETURN_FALSE(marker2);
    } while (marker2 == JPEG_MARKER_PREFIX);  // skip fill bytes

    switch (marker2) {
      // Compressed data escape.
      case 0x00:
        break;
      // Restart
      case JPEG_RST0:
      case JPEG_RST1:
      case JPEG_RST2:
      case JPEG_RST3:
      case JPEG_RST4:
      case JPEG_RST5:
      case JPEG_RST6:
      case JPEG_RST7:
        break;
      case JPEG_EOI: {
        auto buffer_chars = base::as_chars(buffer);
        *eoi_begin_ptr = buffer_chars.subspan(marker1_in_buffer).data();
        *eoi_end_ptr = buffer_chars.subspan(reader.num_read()).data();
        return true;
      }
      default:
        // Skip for other markers.
        uint16_t size;
        READ_U16_OR_RETURN_FALSE(size);
        if (size < sizeof(size)) {
          DLOG(ERROR) << "Ill-formed JPEG. Segment size (" << size
                      << ") is smaller than size field (" << sizeof(size)
                      << ")";
          return false;
        }
        size -= sizeof(size);

        if (!reader.Skip(size)) {
          DLOG(ERROR) << "Ill-formed JPEG. Remaining size ("
                      << reader.remaining()
                      << ") is smaller than header specified (" << size << ")";
          return false;
        }
        break;
    }
  }
  return false;
}

// |result| is already initialized to 0 in ParseJpegPicture.
static bool ParseSOI(base::span<const uint8_t> buffer,
                     JpegParseResult* result) {
  // Spec B.2.1 High-level syntax
  DCHECK(result);
  uint8_t marker1;
  uint8_t marker2;
  bool has_marker_dqt = false;
  bool has_marker_sos = false;

  // Once reached SOS, all necessary data are parsed.
  auto reader = base::SpanReader(buffer);
  while (!has_marker_sos) {
    READ_U8_OR_RETURN_FALSE(marker1);
    if (marker1 != JPEG_MARKER_PREFIX)
      return false;

    do {
      READ_U8_OR_RETURN_FALSE(marker2);
    } while (marker2 == JPEG_MARKER_PREFIX);  // skip fill bytes

    uint16_t size;
    READ_U16_OR_RETURN_FALSE(size);
    // The size includes the size field itself.
    if (size < sizeof(size)) {
      DLOG(ERROR) << "Ill-formed JPEG. Segment size (" << size
                  << ") is smaller than size field (" << sizeof(size) << ")";
      return false;
    }
    size -= sizeof(size);

    if (reader.remaining() < size) {
      DLOG(ERROR) << "Ill-formed JPEG. Remaining size (" << reader.remaining()
                  << ") is smaller than header specified (" << size << ")";
      return false;
    }

    switch (marker2) {
      case JPEG_SOF0:
        if (!ParseSOF(reader.remaining_span().first(size),
                      &result->frame_header)) {
          DLOG(ERROR) << "ParseSOF failed";
          return false;
        }
        break;
      case JPEG_SOF1:
      case JPEG_SOF2:
      case JPEG_SOF3:
      case JPEG_SOF5:
      case JPEG_SOF6:
      case JPEG_SOF7:
      case JPEG_SOF9:
      case JPEG_SOF10:
      case JPEG_SOF11:
      case JPEG_SOF13:
      case JPEG_SOF14:
      case JPEG_SOF15:
        DLOG(ERROR) << "Only SOF0 (baseline) is supported, but got SOF"
                    << (marker2 - JPEG_SOF0);
        return false;
      case JPEG_DQT:
        if (!ParseDQT(reader.remaining_span().first(size), result->q_table)) {
          DLOG(ERROR) << "ParseDQT failed";
          return false;
        }
        has_marker_dqt = true;
        break;
      case JPEG_DHT:
        if (!ParseDHT(reader.remaining_span().first(size), result->dc_table,
                      result->ac_table)) {
          DLOG(ERROR) << "ParseDHT failed";
          return false;
        }
        break;
      case JPEG_DRI:
        if (!ParseDRI(reader.remaining_span().first(size),
                      &result->restart_interval)) {
          DLOG(ERROR) << "ParseDRI failed";
          return false;
        }
        break;
      case JPEG_SOS:
        if (!ParseSOS(reader.remaining_span().first(size), result->frame_header,
                      &result->scan)) {
          DLOG(ERROR) << "ParseSOS failed";
          return false;
        }
        has_marker_sos = true;
        break;
      default:
        DVLOG(4) << "unknown marker " << static_cast<int>(marker2);
        break;
    }
    reader.Skip(size);
  }

  if (!has_marker_dqt) {
    DLOG(ERROR) << "No DQT marker found";
    return false;
  }

  // Scan data follows scan header immediately.
  auto remain = base::as_chars(reader.remaining_span());
  result->data = remain.data();
  result->data_size = remain.size();
  return true;
}

bool ParseJpegPicture(base::span<const uint8_t> buffer,
                      JpegParseResult* result) {
  DCHECK(result);

  auto reader = base::SpanReader(buffer);
  std::ranges::fill(base::byte_span_from_ref(*result), 0u);

  uint8_t marker1;
  uint8_t marker2;
  READ_U8_OR_RETURN_FALSE(marker1);
  READ_U8_OR_RETURN_FALSE(marker2);
  if (marker1 != JPEG_MARKER_PREFIX || marker2 != JPEG_SOI) {
    DLOG(ERROR) << "Not a JPEG";
    return false;
  }

  if (!ParseSOI(reader.remaining_span(), result)) {
    return false;
  }
  base::span<const uint8_t> result_span = base::as_bytes(
      // TODO(crbug.com/40284755): Make this span part of JpegParseResult.
      UNSAFE_TODO(base::span(result->data, result->data_size)));

  // Update the sizes: |result->data_size| should not include the EOI marker or
  // beyond.
  const char* eoi_begin_ptr = nullptr;
  const char* eoi_end_ptr = nullptr;
  if (!SearchEOI(result_span, &eoi_begin_ptr, &eoi_end_ptr)) {
    DLOG(ERROR) << "SearchEOI failed";
    return false;
  }
  DCHECK(eoi_begin_ptr);
  DCHECK(eoi_end_ptr);
  result->data_size = eoi_begin_ptr - result->data;
  result->image_size = eoi_end_ptr - base::as_chars(buffer).data();
  return true;
}

// TODO(andrescj): this function no longer seems necessary. Fix call sites to
// use ParseJpegPicture() directly.
bool ParseJpegStream(base::span<const uint8_t> buffer,
                     JpegParseResult* result) {
  return ParseJpegPicture(buffer, result);
}

}  // namespace media
