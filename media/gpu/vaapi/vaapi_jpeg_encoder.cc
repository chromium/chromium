// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vaapi_jpeg_encoder.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <type_traits>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/jpeg_parser.h"

namespace media {

namespace {

void FillPictureParameters(const gfx::Size& input_size,
                           int quality,
                           VABufferID output_buffer_id,
                           VAEncPictureParameterBufferJPEG* pic_param) {
  pic_param->picture_width = input_size.width();
  pic_param->picture_height = input_size.height();
  pic_param->num_components = 3;

  // Output buffer.
  pic_param->coded_buf = output_buffer_id;
  pic_param->quality = quality;
  // Profile = Baseline.
  pic_param->pic_flags.bits.profile = 0;
  // Sequential encoding.
  pic_param->pic_flags.bits.progressive = 0;
  // Uses Huffman coding.
  pic_param->pic_flags.bits.huffman = 1;
  // Input format is interleaved (YUV).
  pic_param->pic_flags.bits.interleaved = 0;
  // Non-differential Encoding.
  pic_param->pic_flags.bits.differential = 0;
  // Only 8 bit sample depth is currently supported.
  pic_param->sample_bit_depth = 8;
  pic_param->num_scan = 1;
}

void FillQMatrix(VAQMatrixBufferJPEG* q_matrix) {
  // Fill the raw, unscaled quantization tables for libva. The VAAPI driver is
  // responsible for scaling the quantization tables based on picture
  // parameter quality.
  const JpegQuantizationTable& luminance = kDefaultQuantTable[0];
  static_assert(std::extent<decltype(luminance.value)>() ==
                    std::extent<decltype(q_matrix->lum_quantiser_matrix)>(),
                "Luminance quantization table size mismatch.");
  static_assert(std::size(kZigZag8x8) == std::size(luminance.value),
                "Luminance quantization table size mismatch.");
  q_matrix->load_lum_quantiser_matrix = 1;
  for (size_t i = 0; i < std::size(kZigZag8x8); i++) {
    q_matrix->lum_quantiser_matrix[i] = luminance.value[kZigZag8x8[i]];
  }

  const JpegQuantizationTable& chrominance = kDefaultQuantTable[1];
  static_assert(std::extent<decltype(chrominance.value)>() ==
                    std::extent<decltype(q_matrix->chroma_quantiser_matrix)>(),
                "Chrominance quantization table size mismatch.");
  static_assert(std::size(kZigZag8x8) == std::size(chrominance.value),
                "Chrominance quantization table size mismatch.");
  q_matrix->load_chroma_quantiser_matrix = 1;
  for (size_t i = 0; i < std::size(kZigZag8x8); i++) {
    q_matrix->chroma_quantiser_matrix[i] = chrominance.value[kZigZag8x8[i]];
  }
}

void FillHuffmanTableParameters(
    VAHuffmanTableBufferJPEGBaseline* huff_table_param) {
  static_assert(std::size(kDefaultDcTable) == std::size(kDefaultAcTable),
                "DC table and AC table size mismatch.");
  static_assert(std::size(kDefaultDcTable) ==
                    std::extent<decltype(huff_table_param->huffman_table)>(),
                "DC table and destination table size mismatch.");

  for (size_t i = 0; i < std::size(kDefaultDcTable); ++i) {
    const JpegHuffmanTable& dcTable = kDefaultDcTable[i];
    const JpegHuffmanTable& acTable = kDefaultAcTable[i];
    huff_table_param->load_huffman_table[i] = true;

    // Load DC Table.
    SafeArrayMemcpy(huff_table_param->huffman_table[i].num_dc_codes,
                    dcTable.code_length);
    // |code_values| of JpegHuffmanTable needs to hold DC and AC code values
    // so it has different size than
    // |huff_table_param->huffman_table[i].dc_values|. Therefore we can't use
    // SafeArrayMemcpy() here.
    static_assert(
        std::extent<decltype(huff_table_param->huffman_table[i].dc_values)>() <=
            std::extent<decltype(dcTable.code_value)>(),
        "DC table code value array too small.");
    memcpy(huff_table_param->huffman_table[i].dc_values, &dcTable.code_value[0],
           sizeof(huff_table_param->huffman_table[i].dc_values));

    // Load AC Table.
    SafeArrayMemcpy(huff_table_param->huffman_table[i].num_ac_codes,
                    acTable.code_length);
    SafeArrayMemcpy(huff_table_param->huffman_table[i].ac_values,
                    acTable.code_value);

    memset(huff_table_param->huffman_table[i].pad, 0,
           sizeof(huff_table_param->huffman_table[i].pad));
  }
}

void FillSliceParameters(VAEncSliceParameterBufferJPEG* slice_param) {
  slice_param->restart_interval = 0;
  slice_param->num_components = 3;

  slice_param->components[0].component_selector = 1;
  slice_param->components[0].dc_table_selector = 0;
  slice_param->components[0].ac_table_selector = 0;

  slice_param->components[1].component_selector = 2;
  slice_param->components[1].dc_table_selector = 1;
  slice_param->components[1].ac_table_selector = 1;

  slice_param->components[2].component_selector = 3;
  slice_param->components[2].dc_table_selector = 1;
  slice_param->components[2].ac_table_selector = 1;
}

size_t FillJpegHeader(const gfx::Size& input_size,
                      const uint8_t* exif_buffer,
                      size_t exif_buffer_size,
                      int quality,
                      uint8_t* header,
                      size_t* exif_offset) {
  unsigned int width = input_size.width();
  unsigned int height = input_size.height();

  size_t idx = 0;

  // Start Of Input.
  static const uint8_t kSOI[] = {0xFF, JPEG_SOI};
  memcpy(header, kSOI, sizeof(kSOI));
  idx += sizeof(kSOI);

  if (exif_buffer_size > 0) {
    // Application Segment for Exif data.
    uint16_t exif_segment_size = static_cast<uint16_t>(exif_buffer_size + 2);
    const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP1, static_cast<uint8_t>(exif_segment_size / 256),
        static_cast<uint8_t>(exif_segment_size % 256)};
    memcpy(header + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
    *exif_offset = idx;
    memcpy(header + idx, exif_buffer, exif_buffer_size);
    idx += exif_buffer_size;
  } else {
    // Application Segment - JFIF standard 1.01.
    static const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP0, 0x00,
        0x10,  // Segment length:16 (2-byte).
        0x4A,  // J
        0x46,  // F
        0x49,  // I
        0x46,  // F
        0x00,  // 0
        0x01,  // Major version.
        0x01,  // Minor version.
        0x01,  // Density units 0:no units, 1:pixels per inch,
               // 2: pixels per cm.
        0x00,
        0x48,  // X density (2-byte).
        0x00,
        0x48,  // Y density (2-byte).
        0x00,  // Thumbnail width.
        0x00   // Thumbnail height.
    };
    memcpy(header + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
  }

  if (quality <= 0) {
    quality = 1;
  }

  // Normalize quality factor.
  // Unlike VAQMatrixBufferJPEG, we have to scale quantization table in JPEG
  // header by ourselves.
  uint32_t quality_normalized = base::saturated_cast<uint32_t>(
      (quality < 50) ? (5000 / quality) : (200 - (quality * 2)));

  // Quantization Tables.
  for (size_t i = 0; i < 2; ++i) {
    const uint8_t kQuantSegment[] = {
        0xFF, JPEG_DQT, 0x00,
        0x03 + kDctSize,         // Segment length:67 (2-byte).
        static_cast<uint8_t>(i)  // Precision (4-bit high) = 0,
                                 // Index (4-bit low) = i.
    };
    memcpy(header + idx, kQuantSegment, sizeof(kQuantSegment));
    idx += sizeof(kQuantSegment);

    const JpegQuantizationTable& quant_table = kDefaultQuantTable[i];
    for (size_t j = 0; j < kDctSize; ++j) {
      // The iHD media driver shifts the quantization values
      // by 50 while encoding. We should add 50 here to
      // ensure the correctness in the packed header that is
      // directly stuffed into the bitstream as JPEG headers.
      // GStreamer test cases show a psnr improvement in
      // Y plane (41.27 to 48.31) with this quirk.
      const static uint32_t shift =
          VaapiWrapper::GetImplementationType() == VAImplementation::kIntelIHD ? 50 : 0;
      uint32_t scaled_quant_value =
          (quant_table.value[kZigZag8x8[j]] * quality_normalized + shift) / 100;
      scaled_quant_value = std::clamp(scaled_quant_value, 1u, 255u);
      header[idx++] = static_cast<uint8_t>(scaled_quant_value);
    }
  }

  // Start of Frame - Baseline.
  const uint8_t kStartOfFrame[] = {
      0xFF,
      JPEG_SOF0,  // Baseline.
      0x00,
      0x11,  // Segment length:17 (2-byte).
      8,     // Data precision.
      static_cast<uint8_t>((height >> 8) & 0xFF),
      static_cast<uint8_t>(height & 0xFF),
      static_cast<uint8_t>((width >> 8) & 0xFF),
      static_cast<uint8_t>(width & 0xFF),
      0x03,  // Number of Components.
  };
  memcpy(header + idx, kStartOfFrame, sizeof(kStartOfFrame));
  idx += sizeof(kStartOfFrame);
  for (uint8_t i = 0; i < 3; ++i) {
    // These are the values for U and V planes.
    uint8_t h_sample_factor = 1;
    uint8_t v_sample_factor = 1;
    uint8_t quant_table_number = 1;
    if (!i) {
      // These are the values for Y plane.
      h_sample_factor = 2;
      v_sample_factor = 2;
      quant_table_number = 0;
    }

    header[idx++] = i + 1;
    // Horizontal Sample Factor (4-bit high),
    // Vertical Sample Factor (4-bit low).
    header[idx++] = (h_sample_factor << 4) | v_sample_factor;
    header[idx++] = quant_table_number;
  }

  static const uint8_t kDcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0x1F,  // Segment length:31 (2-byte).
  };
  static const uint8_t kAcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0xB5,  // Segment length:181 (2-byte).
  };

  // Huffman Tables.
  for (size_t i = 0; i < 2; ++i) {
    // DC Table.
    memcpy(header + idx, kDcSegment, sizeof(kDcSegment));
    idx += sizeof(kDcSegment);

    // Type (4-bit high) = 0:DC, Index (4-bit low).
    header[idx++] = static_cast<uint8_t>(i);

    const JpegHuffmanTable& dcTable = kDefaultDcTable[i];
    for (size_t j = 0; j < kNumDcRunSizeBits; ++j)
      header[idx++] = dcTable.code_length[j];
    for (size_t j = 0; j < kNumDcCodeWordsHuffVal; ++j)
      header[idx++] = dcTable.code_value[j];

    // AC Table.
    memcpy(header + idx, kAcSegment, sizeof(kAcSegment));
    idx += sizeof(kAcSegment);

    // Type (4-bit high) = 1:AC, Index (4-bit low).
    header[idx++] = 0x10 | static_cast<uint8_t>(i);

    const JpegHuffmanTable& acTable = kDefaultAcTable[i];
    for (size_t j = 0; j < kNumAcRunSizeBits; ++j)
      header[idx++] = acTable.code_length[j];
    for (size_t j = 0; j < kNumAcCodeWordsHuffVal; ++j)
      header[idx++] = acTable.code_value[j];
  }

  // Start of Scan.
  static const uint8_t kStartOfScan[] = {
      0xFF, JPEG_SOS, 0x00,
      0x0C,  // Segment Length:12 (2-byte).
      0x03   // Number of components in scan.
  };
  memcpy(header + idx, kStartOfScan, sizeof(kStartOfScan));
  idx += sizeof(kStartOfScan);

  for (uint8_t i = 0; i < 3; ++i) {
    uint8_t dc_table_number = 1;
    uint8_t ac_table_number = 1;
    if (!i) {
      dc_table_number = 0;
      ac_table_number = 0;
    }

    header[idx++] = i + 1;
    // DC Table Selector (4-bit high), AC Table Selector (4-bit low).
    header[idx++] = (dc_table_number << 4) | ac_table_number;
  }
  header[idx++] = 0x00;  // 0 for Baseline.
  header[idx++] = 0x3F;  // 63 for Baseline.
  header[idx++] = 0x00;  // 0 for Baseline.

  return idx << 3;
}

}  // namespace

VaapiJpegEncoder::VaapiJpegEncoder(scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper),
      q_matrix_cached_(nullptr),
      huff_table_param_cached_(nullptr),
      slice_param_cached_(nullptr) {}

VaapiJpegEncoder::~VaapiJpegEncoder() {}

size_t VaapiJpegEncoder::GetMaxCodedBufferSize(const gfx::Size& size) {
  return size.GetArea() * 3 / 2 + kJpegDefaultHeaderSize;
}

bool VaapiJpegEncoder::Encode(const gfx::Size& input_size,
                              const uint8_t* exif_buffer,
                              size_t exif_buffer_size,
                              int quality,
                              VASurfaceID surface_id,
                              VABufferID output_buffer_id,
                              size_t* exif_offset) {
  DCHECK_NE(surface_id, VA_INVALID_SURFACE);

  if (input_size.width() > kMaxDimension ||
      input_size.height() > kMaxDimension) {
    return false;
  }

  // Set picture parameters.
  VAEncPictureParameterBufferJPEG pic_param;
  FillPictureParameters(input_size, quality, output_buffer_id, &pic_param);

  if (!q_matrix_cached_) {
    q_matrix_cached_.reset(new VAQMatrixBufferJPEG());
    FillQMatrix(q_matrix_cached_.get());
  }

  if (!huff_table_param_cached_) {
    huff_table_param_cached_.reset(new VAHuffmanTableBufferJPEGBaseline());
    FillHuffmanTableParameters(huff_table_param_cached_.get());
  }

  // Set slice parameters.
  if (!slice_param_cached_) {
    slice_param_cached_.reset(new VAEncSliceParameterBufferJPEG());
    FillSliceParameters(slice_param_cached_.get());
  }

  size_t jpeg_header_size =
      exif_buffer_size > 0
          ? kJpegDefaultHeaderSize + kJFIFApp1HeaderSize + exif_buffer_size
          : kJpegDefaultHeaderSize + kJFIFApp0Size;
  std::vector<uint8_t> jpeg_header(jpeg_header_size);
  const size_t length_in_bits =
      FillJpegHeader(input_size, exif_buffer, exif_buffer_size, quality,
                     jpeg_header.data(), exif_offset);

  VAEncPackedHeaderParameterBuffer header_param;
  memset(&header_param, 0, sizeof(header_param));
  header_param.type = VAEncPackedHeaderRawData;
  header_param.bit_length = length_in_bits;
  header_param.has_emulation_bytes = 0;

  if (!vaapi_wrapper_->SubmitBuffers(
          {{VAEncPictureParameterBufferType, sizeof(pic_param), &pic_param},
           {VAQMatrixBufferType, sizeof(*q_matrix_cached_),
            q_matrix_cached_.get()},
           {VAHuffmanTableBufferType, sizeof(*huff_table_param_cached_),
            huff_table_param_cached_.get()},
           {VAEncSliceParameterBufferType, sizeof(*slice_param_cached_),
            slice_param_cached_.get()},
           {VAEncPackedHeaderParameterBufferType, sizeof(header_param),
            &header_param},
           {VAEncPackedHeaderDataBufferType, (length_in_bits + 7) / 8,
            jpeg_header.data()}})) {
    return false;
  }

  // Submit the |surface_id| which contains input YUV frame and begin encoding.
  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(surface_id);
}

}  // namespace media
