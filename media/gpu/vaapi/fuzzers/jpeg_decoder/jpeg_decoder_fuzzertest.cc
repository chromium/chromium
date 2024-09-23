// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <va/va.h>

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/vaapi/fuzzers/jpeg_decoder/jpeg_decoder_fuzzer_input.pb.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/jpeg_parser.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/gfx/geometry/size.h"

// This file defines a fuzzer test for the JPEG decoding functionality of the
// VA-API. To do the fuzzing, we re-use code from media::VaapiJpegDecoder: we
// poke at its internals to get past the parsing and image support validation.
// That way, we can inject a large variety of data into the VA-API driver and
// exercise more paths than if we just pass raw data to the
// media::VaapiJpegDecoder using its public API.
//
// The VA-API does not take raw JPEG data. Instead, it expects that some
// pre-parsing was done. Therefore, we feed the data as a
// media::JpegParseResult. To generate the fuzzing inputs, we use
// libprotobuf-mutator, so we define a protobuf that mirrors the structure of
// media::JpegParseResult.
//
// This fuzzer should be run on a real device that supports the VA-API.
// Additionally, libva and the backend user-space VA-API driver (plus its
// dependencies) should be instrumented for fuzzing.

namespace {

// Converts |proto_huffman_table| to a media::JpegHuffmanTable which can be used
// to populate the Huffman table fields of a media::JpegParseResult.
media::JpegHuffmanTable ConvertToJpegHuffmanTable(
    const media::fuzzing::JpegHuffmanTable& proto_huffman_table) {
  media::JpegHuffmanTable huffman_table{};
  huffman_table.valid = proto_huffman_table.valid();
  memcpy(huffman_table.code_length, proto_huffman_table.code_length().data(),
         std::min(std::size(huffman_table.code_length),
                  proto_huffman_table.code_length().size()));
  memcpy(huffman_table.code_value, proto_huffman_table.code_value().data(),
         std::min(std::size(huffman_table.code_value),
                  proto_huffman_table.code_value().size()));
  return huffman_table;
}

// Converts |proto_parse_result| to a media::JpegParseResult that can be used as
// the input for VaapiJpegDecoder::SubmitBuffers(). The data field of the return
// value will point to the same memory as |proto_parse_result|.data().data(), so
// we assume |proto_parse_result| will live for as long as that data needs to be
// used.
media::JpegParseResult ConvertToJpegParseResult(
    const media::fuzzing::JpegParseResult& proto_parse_result) {
  media::JpegParseResult parse_result{};

  // Convert the frame header.
  parse_result.frame_header.visible_width =
      proto_parse_result.frame_header().visible_width() & 0xFFFF;
  parse_result.frame_header.visible_height =
      proto_parse_result.frame_header().visible_height() & 0xFFFF;
  parse_result.frame_header.coded_width =
      proto_parse_result.frame_header().coded_width() & 0xFFFF;
  parse_result.frame_header.coded_height =
      proto_parse_result.frame_header().coded_height() & 0xFFFF;

  const size_t frame_header_num_components =
      std::min(std::size(parse_result.frame_header.components),
               base::checked_cast<size_t>(
                   proto_parse_result.frame_header().components_size()));
  for (size_t i = 0; i < frame_header_num_components; i++) {
    const auto& input_jpeg_component =
        proto_parse_result.frame_header().components()[i];
    parse_result.frame_header.components[i].id =
        input_jpeg_component.id() & 0xFF;
    parse_result.frame_header.components[i].horizontal_sampling_factor =
        input_jpeg_component.horizontal_sampling_factor() & 0xFF;
    parse_result.frame_header.components[i].vertical_sampling_factor =
        input_jpeg_component.vertical_sampling_factor() & 0xFF;
    parse_result.frame_header.components[i].quantization_table_selector =
        input_jpeg_component.quantization_table_selector() & 0xFF;
  }
  parse_result.frame_header.num_components =
      static_cast<uint8_t>(frame_header_num_components);

  // Convert the DC/AC Huffman tables.
  const size_t num_dc_tables =
      std::min(std::size(parse_result.dc_table),
               base::checked_cast<size_t>(proto_parse_result.dc_table_size()));
  for (size_t i = 0; i < num_dc_tables; i++) {
    parse_result.dc_table[i] =
        ConvertToJpegHuffmanTable(proto_parse_result.dc_table()[i]);
  }
  const size_t num_ac_tables =
      std::min(std::size(parse_result.ac_table),
               base::checked_cast<size_t>(proto_parse_result.ac_table_size()));
  for (size_t i = 0; i < num_ac_tables; i++) {
    parse_result.ac_table[i] =
        ConvertToJpegHuffmanTable(proto_parse_result.ac_table()[i]);
  }

  // Convert the quantization tables.
  const size_t num_q_tables =
      std::min(std::size(parse_result.q_table),
               base::checked_cast<size_t>(proto_parse_result.q_table_size()));
  for (size_t i = 0; i < num_q_tables; i++) {
    const media::fuzzing::JpegQuantizationTable& input_q_table =
        proto_parse_result.q_table()[i];
    parse_result.q_table[i].valid = input_q_table.valid();
    memcpy(parse_result.q_table[i].value, input_q_table.value().data(),
           std::min(std::size(parse_result.q_table[i].value),
                    input_q_table.value().size()));
  }

  // Convert the scan header.
  const size_t scan_num_components = std::min(
      std::size(parse_result.scan.components),
      base::checked_cast<size_t>(proto_parse_result.scan().components_size()));
  for (size_t i = 0; i < scan_num_components; i++) {
    const media::fuzzing::JpegScanHeader::Component& input_component =
        proto_parse_result.scan().components()[i];
    parse_result.scan.components[i].component_selector =
        input_component.component_selector() & 0xFF;
    parse_result.scan.components[i].dc_selector =
        input_component.dc_selector() & 0xFF;
    parse_result.scan.components[i].ac_selector =
        input_component.ac_selector() & 0xFF;
  }
  parse_result.scan.num_components = static_cast<uint8_t>(scan_num_components);

  // Convert the coded data. Note that we don't do a deep copy, so we assume
  // that |proto_parse_result| will live for as long as |parse_result|.data is
  // needed.
  parse_result.data = proto_parse_result.data().data();
  parse_result.data_size = proto_parse_result.data().size();

  // Convert the rest of the fields.
  parse_result.restart_interval =
      proto_parse_result.restart_interval() & 0xFFFF;
  parse_result.image_size =
      base::strict_cast<size_t>(proto_parse_result.image_size());

  return parse_result;
}

unsigned int ConvertToVARTFormat(
    media::fuzzing::VARTFormat proto_picture_va_rt_format) {
  switch (proto_picture_va_rt_format) {
    case media::fuzzing::VARTFormat::INVALID:
      return media::kInvalidVaRtFormat;
    case media::fuzzing::VARTFormat::YUV420:
      return VA_RT_FORMAT_YUV420;
    case media::fuzzing::VARTFormat::YUV422:
      return VA_RT_FORMAT_YUV422;
    case media::fuzzing::VARTFormat::YUV444:
      return VA_RT_FORMAT_YUV444;
    case media::fuzzing::VARTFormat::YUV411:
      return VA_RT_FORMAT_YUV411;
    case media::fuzzing::VARTFormat::YUV400:
      return VA_RT_FORMAT_YUV400;
    case media::fuzzing::VARTFormat::YUV420_10:
      return VA_RT_FORMAT_YUV420_10;
    case media::fuzzing::VARTFormat::YUV422_10:
      return VA_RT_FORMAT_YUV422_10;
    case media::fuzzing::VARTFormat::YUV444_10:
      return VA_RT_FORMAT_YUV444_10;
    case media::fuzzing::VARTFormat::YUV420_12:
      return VA_RT_FORMAT_YUV420_12;
    case media::fuzzing::VARTFormat::YUV422_12:
      return VA_RT_FORMAT_YUV422_12;
    case media::fuzzing::VARTFormat::YUV444_12:
      return VA_RT_FORMAT_YUV444_12;
    case media::fuzzing::VARTFormat::RGB16:
      return VA_RT_FORMAT_RGB16;
    case media::fuzzing::VARTFormat::RGB32:
      return VA_RT_FORMAT_RGB32;
    case media::fuzzing::VARTFormat::RGBP:
      return VA_RT_FORMAT_RGBP;
    case media::fuzzing::VARTFormat::RGB32_10:
      return VA_RT_FORMAT_RGB32_10;
    case media::fuzzing::VARTFormat::PROTECTED:
      return VA_RT_FORMAT_PROTECTED;
  }
}

}  // namespace

namespace media {
namespace fuzzing {

// This wrapper lets us poke at the internals of a media::VaapiJpegDecoder. It
// somewhat mirrors the way media::VaapiImageDecoder drives a VaapiJpegDecoder.
class VaapiJpegDecoderWrapper {
 public:
  VaapiJpegDecoderWrapper() = default;
  ~VaapiJpegDecoderWrapper() = default;

  bool Initialize() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_.decoder_sequence_checker_);
    return decoder_.Initialize(base::DoNothing());
  }

  bool MaybeCreateSurface(unsigned int picture_va_rt_format,
                          const gfx::Size& new_coded_size,
                          const gfx::Size& new_visible_size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_.decoder_sequence_checker_);
    if (!decoder_.MaybeCreateSurface(picture_va_rt_format, new_coded_size,
                                     new_visible_size)) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

  bool SubmitBuffers(const media::JpegParseResult& parse_result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_.decoder_sequence_checker_);
    if (!decoder_.SubmitBuffers(parse_result)) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

  bool ExecuteAndDestroyPendingBuffers() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_.decoder_sequence_checker_);
    if (!decoder_.vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
            decoder_.scoped_va_context_and_surface_->id())) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

 private:
  VaapiJpegDecoder decoder_;
};

struct Environment {
  Environment() { VaapiWrapper::PreSandboxInitialization(); }
};

DEFINE_PROTO_FUZZER(const JpegImageList& image_list) {
  static const Environment env;
  VaapiJpegDecoderWrapper decoder_wrapper;
  if (!decoder_wrapper.Initialize()) {
    LOG(ERROR) << "Cannot initialize the VaapiJpegDecoder";
    abort();
  }
  for (const auto& jpeg_image : image_list.images()) {
    if (!decoder_wrapper.MaybeCreateSurface(
            ConvertToVARTFormat(jpeg_image.picture_va_rt_format()),
            gfx::Size(
                base::strict_cast<int>(jpeg_image.surface_coded_width()),
                base::strict_cast<int>(jpeg_image.surface_coded_height())),
            gfx::Size(
                base::strict_cast<int>(jpeg_image.surface_visible_width()),
                base::strict_cast<int>(jpeg_image.surface_visible_height())))) {
      continue;
    }
    if (!decoder_wrapper.SubmitBuffers(
            ConvertToJpegParseResult(jpeg_image.parse_result()))) {
      continue;
    }
    if (!decoder_wrapper.ExecuteAndDestroyPendingBuffers())
      continue;
    // TODO(crbug.com/1034357): for decodes that succeeded, it would be good to
    // get the result as a dma-buf, map it, and read it to make sure that doing
    // so does not lead us into an invalid state.
  }
}

}  // namespace fuzzing
}  // namespace media
