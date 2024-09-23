// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"

#include <string.h>
#include <va/va.h>

#include <iostream>
#include <type_traits>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/jpeg_parser.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

static void FillPictureParameters(
    const JpegFrameHeader& frame_header,
    VAPictureParameterBufferJPEGBaseline* pic_param) {
  pic_param->picture_width = frame_header.coded_width;
  pic_param->picture_height = frame_header.coded_height;
  pic_param->num_components = frame_header.num_components;

  for (int i = 0; i < pic_param->num_components; i++) {
    pic_param->components[i].component_id = frame_header.components[i].id;
    pic_param->components[i].h_sampling_factor =
        frame_header.components[i].horizontal_sampling_factor;
    pic_param->components[i].v_sampling_factor =
        frame_header.components[i].vertical_sampling_factor;
    pic_param->components[i].quantiser_table_selector =
        frame_header.components[i].quantization_table_selector;
  }
}

static void FillIQMatrix(const JpegQuantizationTable* q_table,
                         VAIQMatrixBufferJPEGBaseline* iq_matrix) {
  static_assert(kJpegMaxQuantizationTableNum ==
                    std::extent<decltype(iq_matrix->load_quantiser_table)>(),
                "max number of quantization table mismatched");
  static_assert(
      sizeof(iq_matrix->quantiser_table[0]) == sizeof(q_table[0].value),
      "number of quantization entries mismatched");
  for (size_t i = 0; i < kJpegMaxQuantizationTableNum; i++) {
    if (!q_table[i].valid)
      continue;
    iq_matrix->load_quantiser_table[i] = 1;
    for (size_t j = 0; j < std::size(q_table[i].value); j++)
      iq_matrix->quantiser_table[i][j] = q_table[i].value[j];
  }
}

static void FillHuffmanTable(const JpegHuffmanTable* dc_table,
                             const JpegHuffmanTable* ac_table,
                             VAHuffmanTableBufferJPEGBaseline* huffman_table) {
  // Use default huffman tables if not specified in header.
  bool has_huffman_table = false;
  for (size_t i = 0; i < kJpegMaxHuffmanTableNumBaseline; i++) {
    if (dc_table[i].valid || ac_table[i].valid) {
      has_huffman_table = true;
      break;
    }
  }
  if (!has_huffman_table) {
    dc_table = kDefaultDcTable;
    ac_table = kDefaultAcTable;
  }

  static_assert(kJpegMaxHuffmanTableNumBaseline ==
                    std::extent<decltype(huffman_table->load_huffman_table)>(),
                "max number of huffman table mismatched");
  static_assert(sizeof(huffman_table->huffman_table[0].num_dc_codes) ==
                    sizeof(dc_table[0].code_length),
                "size of huffman table code length mismatch");
  static_assert(sizeof(huffman_table->huffman_table[0].dc_values[0]) ==
                    sizeof(dc_table[0].code_value[0]),
                "size of huffman table code value mismatch");
  for (size_t i = 0; i < kJpegMaxHuffmanTableNumBaseline; i++) {
    if (!dc_table[i].valid || !ac_table[i].valid)
      continue;
    huffman_table->load_huffman_table[i] = 1;

    memcpy(huffman_table->huffman_table[i].num_dc_codes,
           dc_table[i].code_length,
           sizeof(huffman_table->huffman_table[i].num_dc_codes));
    memcpy(huffman_table->huffman_table[i].dc_values, dc_table[i].code_value,
           sizeof(huffman_table->huffman_table[i].dc_values));
    memcpy(huffman_table->huffman_table[i].num_ac_codes,
           ac_table[i].code_length,
           sizeof(huffman_table->huffman_table[i].num_ac_codes));
    memcpy(huffman_table->huffman_table[i].ac_values, ac_table[i].code_value,
           sizeof(huffman_table->huffman_table[i].ac_values));
  }
}

static void FillSliceParameters(
    const JpegParseResult& parse_result,
    VASliceParameterBufferJPEGBaseline* slice_param) {
  slice_param->slice_data_size = parse_result.data_size;
  slice_param->slice_data_offset = 0;
  slice_param->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param->slice_horizontal_position = 0;
  slice_param->slice_vertical_position = 0;
  slice_param->num_components = parse_result.scan.num_components;
  for (int i = 0; i < slice_param->num_components; i++) {
    slice_param->components[i].component_selector =
        parse_result.scan.components[i].component_selector;
    slice_param->components[i].dc_table_selector =
        parse_result.scan.components[i].dc_selector;
    slice_param->components[i].ac_table_selector =
        parse_result.scan.components[i].ac_selector;
  }
  slice_param->restart_interval = parse_result.restart_interval;

  // Cast to int to prevent overflow.
  int max_h_factor =
      parse_result.frame_header.components[0].horizontal_sampling_factor;
  int max_v_factor =
      parse_result.frame_header.components[0].vertical_sampling_factor;
  int mcu_cols = parse_result.frame_header.coded_width / (max_h_factor * 8);
  DCHECK_GT(mcu_cols, 0);
  int mcu_rows = parse_result.frame_header.coded_height / (max_v_factor * 8);
  DCHECK_GT(mcu_rows, 0);
  slice_param->num_mcus = mcu_rows * mcu_cols;
}

// VAAPI only supports a subset of JPEG profiles. This function determines
// whether a given parsed JPEG result is supported or not.
static bool IsVaapiSupportedJpeg(const JpegParseResult& jpeg) {
  // Make sure the JPEG's chroma subsampling format is supported.
  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(
          VAProfileJPEGBaseline, VaSurfaceFormatForJpeg(jpeg.frame_header))) {
    DLOG(ERROR) << "The JPEG's subsampling format is unsupported";
    return false;
  }

  // Validate the visible size.
  if (jpeg.frame_header.visible_width == 0u) {
    DLOG(ERROR) << "Visible width can't be zero";
    return false;
  }
  if (jpeg.frame_header.visible_height == 0u) {
    DLOG(ERROR) << "Visible height can't be zero";
    return false;
  }

  // Validate the coded size.
  gfx::Size min_jpeg_resolution;
  gfx::Size max_jpeg_resolution;
  if (!VaapiWrapper::GetSupportedResolutions(
          VAProfileJPEGBaseline, VaapiWrapper::CodecMode::kDecode,
          min_jpeg_resolution, max_jpeg_resolution)) {
    DLOG(ERROR) << "Could not get the minimum and maximum resolutions";
    return false;
  }
  const int actual_jpeg_coded_width =
      base::strict_cast<int>(jpeg.frame_header.coded_width);
  const int actual_jpeg_coded_height =
      base::strict_cast<int>(jpeg.frame_header.coded_height);
  if (actual_jpeg_coded_width < min_jpeg_resolution.width() ||
      actual_jpeg_coded_height < min_jpeg_resolution.height() ||
      actual_jpeg_coded_width > max_jpeg_resolution.width() ||
      actual_jpeg_coded_height > max_jpeg_resolution.height()) {
    DLOG(ERROR) << "VAAPI doesn't support size " << actual_jpeg_coded_width
                << "x" << actual_jpeg_coded_height << ": not in range "
                << min_jpeg_resolution.ToString() << " - "
                << max_jpeg_resolution.ToString();
    return false;
  }

  return true;
}

}  // namespace

unsigned int VaSurfaceFormatForJpeg(const JpegFrameHeader& frame_header) {
  if (frame_header.num_components != 3)
    return kInvalidVaRtFormat;

  const uint8_t y_h = frame_header.components[0].horizontal_sampling_factor;
  const uint8_t y_v = frame_header.components[0].vertical_sampling_factor;
  const uint8_t u_h = frame_header.components[1].horizontal_sampling_factor;
  const uint8_t u_v = frame_header.components[1].vertical_sampling_factor;
  const uint8_t v_h = frame_header.components[2].horizontal_sampling_factor;
  const uint8_t v_v = frame_header.components[2].vertical_sampling_factor;

  if (u_h != 1 || u_v != 1 || v_h != 1 || v_v != 1)
    return kInvalidVaRtFormat;

  if (y_h == 2 && y_v == 2)
    return VA_RT_FORMAT_YUV420;
  else if (y_h == 2 && y_v == 1)
    return VA_RT_FORMAT_YUV422;
  else if (y_h == 1 && y_v == 1)
    return VA_RT_FORMAT_YUV444;
  return kInvalidVaRtFormat;
}

VaapiJpegDecoder::VaapiJpegDecoder()
    : VaapiImageDecoder(VAProfileJPEGBaseline) {}

VaapiJpegDecoder::~VaapiJpegDecoder() = default;

VaapiImageDecodeStatus VaapiJpegDecoder::AllocateVASurfaceAndSubmitVABuffers(
    base::span<const uint8_t> encoded_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(vaapi_wrapper_);

  // Parse the JPEG encoded data.
  JpegParseResult parse_result;
  if (!ParseJpegPicture(encoded_image, &parse_result)) {
    VLOGF(1) << "ParseJpegPicture failed";
    return VaapiImageDecodeStatus::kParseFailed;
  }

  // Figure out the right format for the VaSurface.
  const unsigned int picture_va_rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  if (picture_va_rt_format == kInvalidVaRtFormat) {
    VLOGF(1) << "Unsupported subsampling";
    return VaapiImageDecodeStatus::kUnsupportedSubsampling;
  }

  // Make sure this JPEG can be decoded.
  if (!IsVaapiSupportedJpeg(parse_result)) {
    VLOGF(1) << "The supplied JPEG is unsupported";
    return VaapiImageDecodeStatus::kUnsupportedImage;
  }

  // Prepare the surface for decoding.
  const gfx::Size new_visible_size(
      base::strict_cast<int>(parse_result.frame_header.visible_width),
      base::strict_cast<int>(parse_result.frame_header.visible_height));
  const gfx::Size new_coded_size(
      base::strict_cast<int>(parse_result.frame_header.coded_width),
      base::strict_cast<int>(parse_result.frame_header.coded_height));
  if (!MaybeCreateSurface(picture_va_rt_format, new_coded_size,
                          new_visible_size)) {
    return VaapiImageDecodeStatus::kSurfaceCreationFailed;
  }

  // Submit input buffers.
  if (!SubmitBuffers(parse_result))
    return VaapiImageDecodeStatus::kSubmitVABuffersFailed;

  return VaapiImageDecodeStatus::kSuccess;
}

gpu::ImageDecodeAcceleratorType VaapiJpegDecoder::GetType() const {
  return gpu::ImageDecodeAcceleratorType::kJpeg;
}

SkYUVColorSpace VaapiJpegDecoder::GetYUVColorSpace() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  return SkYUVColorSpace::kJPEG_SkYUVColorSpace;
}

// static
std::optional<gpu::ImageDecodeAcceleratorSupportedProfile>
VaapiJpegDecoder::GetSupportedProfile() {
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration)) {
    return std::nullopt;
  }
  if (VaapiWrapper::GetImplementationType() == VAImplementation::kMesaGallium) {
    // TODO(crbug.com/40632250): we can't advertise accelerated image decoding
    // in AMD until we support VAAPI surfaces with multiple buffer objects.
    return std::nullopt;
  }

  gpu::ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = gpu::ImageDecodeAcceleratorType::kJpeg;

  const bool got_supported_resolutions = VaapiWrapper::GetSupportedResolutions(
      VAProfileJPEGBaseline, VaapiWrapper::CodecMode::kDecode,
      profile.min_encoded_dimensions, profile.max_encoded_dimensions);
  if (!got_supported_resolutions) {
    return std::nullopt;
  }

  // TODO(andrescj): Ideally, we would advertise support for all the formats
  // supported by the driver. However, for now, we will only support exposing
  // YUV 4:2:0 surfaces as DmaBufs.
  CHECK(VaapiWrapper::GetDecodeSupportedInternalFormats(VAProfileJPEGBaseline)
            .yuv420);
  profile.subsamplings.push_back(gpu::ImageDecodeAcceleratorSubsampling::k420);
  return profile;
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoder::GetImage(
    uint32_t preferred_image_fourcc,
    VaapiImageDecodeStatus* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  if (!scoped_va_context_and_surface_) {
    VLOGF(1) << "No decoded JPEG available";
    *status = VaapiImageDecodeStatus::kInvalidState;
    return nullptr;
  }

  DCHECK(scoped_va_context_and_surface_->IsValid());
  DCHECK(vaapi_wrapper_);
  uint32_t image_fourcc;
  if (!VaapiWrapper::GetJpegDecodeSuitableImageFourCC(
          scoped_va_context_and_surface_->format(), preferred_image_fourcc,
          &image_fourcc)) {
    VLOGF(1) << "Cannot determine the output FOURCC";
    *status = VaapiImageDecodeStatus::kCannotGetImage;
    return nullptr;
  }
  const VAImageFormat image_format{.fourcc = image_fourcc};
  // In at least one driver, the VPP seems to have problems if we request a
  // VAImage with odd dimensions. Rather than debugging the issue in depth, we
  // disable support for odd dimensions since the VAImage path is only expected
  // to be used in camera captures (and we don't expect JPEGs with odd
  // dimensions in that path).
  CHECK((scoped_va_context_and_surface_->size().width() & 1) == 0 &&
        (scoped_va_context_and_surface_->size().height() & 1) == 0)
      << "Getting images with odd dimensions is not supported";
  auto scoped_image = vaapi_wrapper_->CreateVaImage(
      scoped_va_context_and_surface_->id(), image_format,
      scoped_va_context_and_surface_->size());
  if (!scoped_image) {
    VLOGF(1) << "Cannot get VAImage, FOURCC = "
             << FourccToString(image_format.fourcc);
    *status = VaapiImageDecodeStatus::kCannotGetImage;
    return nullptr;
  }

  *status = VaapiImageDecodeStatus::kSuccess;
  return scoped_image;
}

bool VaapiJpegDecoder::MaybeCreateSurface(unsigned int picture_va_rt_format,
                                          const gfx::Size& new_coded_size,
                                          const gfx::Size& new_visible_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(!scoped_va_context_and_surface_ ||
         scoped_va_context_and_surface_->IsValid());
  if (scoped_va_context_and_surface_ &&
      new_visible_size == scoped_va_context_and_surface_->size() &&
      picture_va_rt_format == scoped_va_context_and_surface_->format()) {
    // No need to allocate a new surface. We can re-use the current one.
    return true;
  }

  scoped_va_context_and_surface_.reset();

  // We'll request a surface of |new_coded_size| from the VAAPI, but we will
  // keep track of the |new_visible_size| inside the ScopedVASurface so that
  // when we create a VAImage or export the surface as a NativePixmapDmaBuf, we
  // can report the size that clients should be using to read the contents.
  auto scoped_va_surfaces = vaapi_wrapper_->CreateContextAndScopedVASurfaces(
      picture_va_rt_format, new_coded_size,
      {VaapiWrapper::SurfaceUsageHint::kGeneric}, 1u, new_visible_size);
  if (scoped_va_surfaces.empty()) {
    VLOGF(1) << "CreateContextAndScopedVASurface() failed";
    return false;
  }

  scoped_va_context_and_surface_.reset(scoped_va_surfaces[0].release());
  DCHECK(scoped_va_context_and_surface_->IsValid());
  return true;
}

bool VaapiJpegDecoder::SubmitBuffers(const JpegParseResult& parse_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  // Set picture parameters.
  VAPictureParameterBufferJPEGBaseline pic_param{};
  FillPictureParameters(parse_result.frame_header, &pic_param);

  // Set quantization table.
  VAIQMatrixBufferJPEGBaseline iq_matrix{};
  FillIQMatrix(parse_result.q_table, &iq_matrix);

  // Set huffman table.
  VAHuffmanTableBufferJPEGBaseline huffman_table{};
  FillHuffmanTable(parse_result.dc_table, parse_result.ac_table,
                   &huffman_table);

  // Set slice parameters.
  VASliceParameterBufferJPEGBaseline slice_param{};
  FillSliceParameters(parse_result, &slice_param);

  return vaapi_wrapper_->SubmitBuffers(
      {{VAPictureParameterBufferType, sizeof(pic_param), &pic_param},
       {VAIQMatrixBufferType, sizeof(iq_matrix), &iq_matrix},
       {VAHuffmanTableBufferType, sizeof(huffman_table), &huffman_table},
       {VASliceParameterBufferType, sizeof(slice_param), &slice_param},
       {VASliceDataBufferType, parse_result.data_size,
        const_cast<char*>(parse_result.data)}});
}

}  // namespace media
