// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <utility>

#include <va/va.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {

namespace {
// UMA errors that the VaapiJpegDecodeAccelerator class reports.
enum VAJDADecoderFailure {
  VAAPI_ERROR = 0,
  VAJDA_DECODER_FAILURES_MAX,
};

static void ReportToUMA(VAJDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.DecoderFailure", failure,
                            VAJDA_DECODER_FAILURES_MAX + 1);
}

// Check the value of VA_FOURCC_YUYV, as we don't have access to the VA_FOURCC
// macro in the header file without pulling in the entire <va/va.h>.
static_assert(VA_FOURCC_YUYV == VA_FOURCC('Y', 'U', 'Y', 'V'),
              "VA_FOURCC_YUYV must be equal to VA_FOURCC('Y', 'U', 'Y', 'V')");
constexpr VAImageFormat kImageFormatI420 = {.fourcc = VA_FOURCC_I420,
                                            .byte_order = VA_LSB_FIRST,
                                            .bits_per_pixel = 12};
constexpr VAImageFormat kImageFormatYUYV = {.fourcc = VA_FOURCC_YUYV,
                                            .byte_order = VA_LSB_FIRST,
                                            .bits_per_pixel = 16};

// Convert the specified surface format to the associated output image format.
bool VaSurfaceFormatToImageFormat(uint32_t va_rt_format,
                                  VAImageFormat* va_image_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      *va_image_format = kImageFormatI420;
      return true;
    case VA_RT_FORMAT_YUV422:
      *va_image_format = kImageFormatYUYV;
      return true;
    default:
      return false;
  }
}

static unsigned int VaSurfaceFormatForJpeg(
    const JpegFrameHeader& frame_header) {
  // The range of sampling factor is [1, 4]. Pack them into integer to make the
  // matching code simpler. For example, 0x211 means the sampling factor are 2,
  // 1, 1 for 3 components.
  unsigned int h = 0, v = 0;
  for (int i = 0; i < frame_header.num_components; i++) {
    DCHECK_LE(frame_header.components[i].horizontal_sampling_factor, 4);
    DCHECK_LE(frame_header.components[i].vertical_sampling_factor, 4);
    h = h << 4 | frame_header.components[i].horizontal_sampling_factor;
    v = v << 4 | frame_header.components[i].vertical_sampling_factor;
  }

  switch (frame_header.num_components) {
    case 1:  // Grey image
      return VA_RT_FORMAT_YUV400;

    case 3:  // Y Cb Cr color image
      // See https://en.wikipedia.org/wiki/Chroma_subsampling for the
      // definition of these numbers.
      if (h == 0x211 && v == 0x211)
        return VA_RT_FORMAT_YUV420;

      if (h == 0x211 && v == 0x111)
        return VA_RT_FORMAT_YUV422;

      if (h == 0x111 && v == 0x111)
        return VA_RT_FORMAT_YUV444;

      if (h == 0x411 && v == 0x111)
        return VA_RT_FORMAT_YUV411;
  }
  VLOGF(1) << "Unsupported sampling factor: num_components="
           << frame_header.num_components << ", h=" << std::hex << h
           << ", v=" << v;

  return 0;
}

// VAAPI only supports a subset of JPEG profiles. This function determines
// whether a given parsed JPEG result is supported or not.
static bool IsVaapiSupportedJpeg(const JpegParseResult& jpeg) {
  if (jpeg.frame_header.visible_width < 1 ||
      jpeg.frame_header.visible_height < 1) {
    DLOG(ERROR) << "width(" << jpeg.frame_header.visible_width
                << ") and height(" << jpeg.frame_header.visible_height
                << ") should be at least 1";
    return false;
  }

  // Size 64k*64k is the maximum in the JPEG standard. VAAPI doesn't support
  // resolutions larger than 16k*16k.
  const int kMaxDimension = 16384;
  if (jpeg.frame_header.coded_width > kMaxDimension ||
      jpeg.frame_header.coded_height > kMaxDimension) {
    DLOG(ERROR) << "VAAPI doesn't support size("
                << jpeg.frame_header.coded_width << "*"
                << jpeg.frame_header.coded_height << ") larger than "
                << kMaxDimension << "*" << kMaxDimension;
    return false;
  }

  if (jpeg.frame_header.num_components != 3) {
    DLOG(ERROR) << "VAAPI doesn't support num_components("
                << static_cast<int>(jpeg.frame_header.num_components)
                << ") != 3";
    return false;
  }

  if (jpeg.frame_header.components[0].horizontal_sampling_factor <
          jpeg.frame_header.components[1].horizontal_sampling_factor ||
      jpeg.frame_header.components[0].horizontal_sampling_factor <
          jpeg.frame_header.components[2].horizontal_sampling_factor) {
    DLOG(ERROR) << "VAAPI doesn't supports horizontal sampling factor of Y"
                << " smaller than Cb and Cr";
    return false;
  }

  if (jpeg.frame_header.components[0].vertical_sampling_factor <
          jpeg.frame_header.components[1].vertical_sampling_factor ||
      jpeg.frame_header.components[0].vertical_sampling_factor <
          jpeg.frame_header.components[2].vertical_sampling_factor) {
    DLOG(ERROR) << "VAAPI doesn't supports vertical sampling factor of Y"
                << " smaller than Cb and Cr";
    return false;
  }

  return true;
}

static void FillPictureParameters(
    const JpegFrameHeader& frame_header,
    VAPictureParameterBufferJPEGBaseline* pic_param) {
  memset(pic_param, 0, sizeof(*pic_param));
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
  memset(iq_matrix, 0, sizeof(*iq_matrix));
  static_assert(kJpegMaxQuantizationTableNum ==
                    base::size(decltype(iq_matrix->load_quantiser_table){}),
                "max number of quantization table mismatched");
  static_assert(
      sizeof(iq_matrix->quantiser_table[0]) == sizeof(q_table[0].value),
      "number of quantization entries mismatched");
  for (size_t i = 0; i < kJpegMaxQuantizationTableNum; i++) {
    if (!q_table[i].valid)
      continue;
    iq_matrix->load_quantiser_table[i] = 1;
    for (size_t j = 0; j < base::size(q_table[i].value); j++)
      iq_matrix->quantiser_table[i][j] = q_table[i].value[j];
  }
}

static void FillHuffmanTable(const JpegHuffmanTable* dc_table,
                             const JpegHuffmanTable* ac_table,
                             VAHuffmanTableBufferJPEGBaseline* huffman_table) {
  memset(huffman_table, 0, sizeof(*huffman_table));
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
                    base::size(decltype(huffman_table->load_huffman_table){}),
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
  memset(slice_param, 0, sizeof(*slice_param));
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

}  // namespace

void VaapiJpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                             Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegDecodeAccelerator::NotifyError,
                                  weak_this_factory_.GetWeakPtr(),
                                  bitstream_buffer_id, error));
    return;
  }
  VLOGF(1) << "Notifying of error " << error;
  DCHECK(client_);
  client_->NotifyError(bitstream_buffer_id, error);
}

void VaapiJpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(bitstream_buffer_id);
}

VaapiJpegDecodeAccelerator::VaapiJpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      decoder_thread_("VaapiJpegDecoderThread"),
      va_surface_id_(VA_INVALID_SURFACE),
      va_rt_format_(0),
      weak_this_factory_(this) {}

VaapiJpegDecodeAccelerator::~VaapiJpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiJpegDecodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  decoder_thread_.Stop();
}

bool VaapiJpegDecodeAccelerator::Initialize(Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  client_ = client;

  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, VAProfileJPEGBaseline,
                           base::Bind(&ReportToUMA, VAAPI_ERROR));

  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI";
    return false;
  }

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

bool VaapiJpegDecodeAccelerator::OutputPicture(
    VASurfaceID va_surface_id,
    uint32_t va_surface_format,
    int32_t input_buffer_id,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", "VaapiJpegDecodeAccelerator::OutputPicture",
               "input_buffer_id", input_buffer_id);

  DVLOGF(4) << "Outputting VASurface " << va_surface_id
            << " into video_frame associated with input buffer id "
            << input_buffer_id;

  // Specify which image format we will request from the VAAPI. As the expected
  // output format is I420, we will first try this format. If converting to I420
  // is not supported by the decoder, we will request the image in its original
  // chroma sampling format.
  VAImageFormat va_image_format = kImageFormatI420;
  if (!VaapiWrapper::IsImageFormatSupported(va_image_format)) {
    if (!VaSurfaceFormatToImageFormat(va_surface_format, &va_image_format)) {
      VLOGF(1) << "Unsupported surface format";
      return false;
    }
  }

  const gfx::Size coded_size = video_frame->coded_size();
  auto scoped_image = vaapi_wrapper_->CreateVaImage(
      va_surface_id, &va_image_format, coded_size);
  if (!scoped_image) {
    VLOGF(1) << "Cannot get VAImage";
    return false;
  }
  const VAImage* image = scoped_image->image();
  auto* mem = static_cast<uint8_t*>(scoped_image->va_buffer()->data());

  // Copy image content from VAImage to VideoFrame. If the image is not in the
  // I420 format we'll have to convert it.
  DCHECK_GE(image->width, coded_size.width());
  DCHECK_GE(image->height, coded_size.height());
  uint8_t* dst_y = video_frame->data(VideoFrame::kYPlane);
  uint8_t* dst_u = video_frame->data(VideoFrame::kUPlane);
  uint8_t* dst_v = video_frame->data(VideoFrame::kVPlane);
  size_t dst_y_stride = video_frame->stride(VideoFrame::kYPlane);
  size_t dst_u_stride = video_frame->stride(VideoFrame::kUPlane);
  size_t dst_v_stride = video_frame->stride(VideoFrame::kVPlane);

  switch (va_image_format.fourcc) {
    case VA_FOURCC_I420: {
      DCHECK_EQ(image->num_planes, 3u);
      const uint8_t* src_y = mem + image->offsets[0];
      const uint8_t* src_u = mem + image->offsets[1];
      const uint8_t* src_v = mem + image->offsets[2];
      const size_t src_y_stride = image->pitches[0];
      const size_t src_u_stride = image->pitches[1];
      const size_t src_v_stride = image->pitches[2];
      if (libyuv::I420Copy(src_y, src_y_stride, src_u, src_u_stride, src_v,
                           src_v_stride, dst_y, dst_y_stride, dst_u,
                           dst_u_stride, dst_v, dst_v_stride,
                           coded_size.width(), coded_size.height())) {
        VLOGF(1) << "I420Copy failed";
        return false;
      }
      break;
    }
    case VA_FOURCC_YUY2:
    case VA_FOURCC_YUYV: {
      DCHECK_EQ(image->num_planes, 1u);
      const uint8_t* src_yuy2 = mem + image->offsets[0];
      const size_t src_yuy2_stride = image->pitches[0];
      if (libyuv::YUY2ToI420(src_yuy2, src_yuy2_stride, dst_y, dst_y_stride,
                             dst_u, dst_u_stride, dst_v, dst_v_stride,
                             coded_size.width(), coded_size.height())) {
        VLOGF(1) << "YUY2ToI420 failed";
        return false;
      }
      break;
    }
    default:
      VLOGF(1) << "Can't convert image to I420: unsupported format 0x"
               << std::hex << va_image_format.fourcc;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiJpegDecodeAccelerator::VideoFrameReady,
                     weak_this_factory_.GetWeakPtr(), input_buffer_id));

  return true;
}

void VaapiJpegDecodeAccelerator::DecodeTask(
    int32_t bitstream_buffer_id,
    std::unique_ptr<UnalignedSharedMemory> shm,
    scoped_refptr<VideoFrame> video_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", "DecodeTask");

  JpegParseResult parse_result;
  if (!ParseJpegPicture(static_cast<const uint8_t*>(shm->memory()), shm->size(),
                        &parse_result)) {
    VLOGF(1) << "ParseJpegPicture failed";
    NotifyError(bitstream_buffer_id, PARSE_JPEG_FAILED);
    return;
  }

  const uint32_t picture_va_rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  if (!picture_va_rt_format) {
    VLOGF(1) << "Unsupported subsampling";
    NotifyError(bitstream_buffer_id, UNSUPPORTED_JPEG);
    return;
  }

  // Reuse VASurface if size doesn't change.
  gfx::Size new_coded_size(parse_result.frame_header.coded_width,
                           parse_result.frame_header.coded_height);
  if (new_coded_size != coded_size_ || va_surface_id_ == VA_INVALID_SURFACE ||
      picture_va_rt_format != va_rt_format_) {
    vaapi_wrapper_->DestroySurfaces();
    va_surface_id_ = VA_INVALID_SURFACE;
    va_rt_format_ = picture_va_rt_format;

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateSurfaces(va_rt_format_, new_coded_size, 1,
                                        &va_surfaces)) {
      VLOGF(1) << "Create VA surface failed";
      NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    coded_size_ = new_coded_size;
  }

  if (!DoDecode(vaapi_wrapper_.get(), parse_result, va_surface_id_)) {
    VLOGF(1) << "Decode JPEG failed";
    NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
    return;
  }

  if (!OutputPicture(va_surface_id_, picture_va_rt_format, bitstream_buffer_id,
                     video_frame)) {
    VLOGF(1) << "Output picture failed";
    NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
    return;
  }
}

void VaapiJpegDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", "Decode", "input_id", bitstream_buffer.id());

  DVLOGF(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
            << " size: " << bitstream_buffer.size();

  // UnalignedSharedMemory will take over the |bitstream_buffer.handle()|.
  auto shm = std::make_unique<UnalignedSharedMemory>(
      bitstream_buffer.handle(), bitstream_buffer.size(), true);

  if (bitstream_buffer.id() < 0) {
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    NotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  if (!shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size())) {
    VLOGF(1) << "Failed to map input buffer";
    NotifyError(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiJpegDecodeAccelerator::DecodeTask,
                                base::Unretained(this), bitstream_buffer.id(),
                                std::move(shm), std::move(video_frame)));
}

bool VaapiJpegDecodeAccelerator::IsSupported() {
  return VaapiWrapper::IsJpegDecodeSupported();
}

// static
bool VaapiJpegDecodeAccelerator::DoDecode(VaapiWrapper* vaapi_wrapper,
                                          const JpegParseResult& parse_result,
                                          VASurfaceID va_surface) {
  DCHECK_NE(va_surface, VA_INVALID_SURFACE);
  if (!IsVaapiSupportedJpeg(parse_result))
    return false;

  // Set picture parameters.
  VAPictureParameterBufferJPEGBaseline pic_param;
  FillPictureParameters(parse_result.frame_header, &pic_param);
  if (!vaapi_wrapper->SubmitBuffer(VAPictureParameterBufferType, &pic_param)) {
    return false;
  }

  // Set quantization table.
  VAIQMatrixBufferJPEGBaseline iq_matrix;
  FillIQMatrix(parse_result.q_table, &iq_matrix);
  if (!vaapi_wrapper->SubmitBuffer(VAIQMatrixBufferType, &iq_matrix)) {
    return false;
  }

  // Set huffman table.
  VAHuffmanTableBufferJPEGBaseline huffman_table;
  FillHuffmanTable(parse_result.dc_table, parse_result.ac_table,
                   &huffman_table);
  if (!vaapi_wrapper->SubmitBuffer(VAHuffmanTableBufferType, &huffman_table)) {
    return false;
  }

  // Set slice parameters.
  VASliceParameterBufferJPEGBaseline slice_param;
  FillSliceParameters(parse_result, &slice_param);
  if (!vaapi_wrapper->SubmitBuffer(VASliceParameterBufferType, &slice_param)) {
    return false;
  }

  // Set scan data.
  if (!vaapi_wrapper->SubmitBuffer(VASliceDataBufferType,
                                   parse_result.data_size,
                                   const_cast<char*>(parse_result.data))) {
    return false;
  }

  return vaapi_wrapper->ExecuteAndDestroyPendingBuffers(va_surface);
}

}  // namespace media
