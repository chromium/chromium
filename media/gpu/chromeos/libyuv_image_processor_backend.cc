// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/libyuv_image_processor_backend.h"

#include "base/memory/ptr_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/libyuv/include/libyuv/rotate.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace media {

namespace {

// TODO(https://bugs.chromium.org/p/libyuv/issues/detail?id=840): Remove
// this once libyuv implements NV12Rotate() and use the libyuv::NV12Rotate().
bool NV12Rotate(uint8_t* tmp_buffer,
                const uint8_t* src_y,
                int src_stride_y,
                const uint8_t* src_uv,
                int src_stride_uv,
                int src_width,
                int src_height,
                uint8_t* dst_y,
                int dst_stride_y,
                uint8_t* dst_uv,
                int dst_stride_uv,
                int dst_width,
                int dst_height,
                VideoRotation relative_rotation) {
  libyuv::RotationModeEnum rotation = libyuv::kRotate0;
  switch (relative_rotation) {
    case VIDEO_ROTATION_0:
      NOTREACHED() << "Unexpected rotation: " << rotation;
      return false;
    case VIDEO_ROTATION_90:
      rotation = libyuv::kRotate90;
      break;
    case VIDEO_ROTATION_180:
      rotation = libyuv::kRotate180;
      break;
    case VIDEO_ROTATION_270:
      rotation = libyuv::kRotate270;
      break;
  }

  // Rotating.
  const int tmp_uv_width = (dst_width + 1) / 2;
  const int tmp_uv_height = (dst_height + 1) / 2;
  uint8_t* const tmp_u = tmp_buffer;
  uint8_t* const tmp_v = tmp_u + tmp_uv_width * tmp_uv_height;

  // Rotate the NV12 planes to I420.
  int ret = libyuv::NV12ToI420Rotate(
      src_y, src_stride_y, src_uv, src_stride_uv, dst_y, dst_stride_y, tmp_u,
      tmp_uv_width, tmp_v, tmp_uv_width, src_width, src_height, rotation);
  if (ret != 0)
    return false;

  // Merge the UV planes into the destination.
  libyuv::MergeUVPlane(tmp_u, tmp_uv_width, tmp_v, tmp_uv_width, dst_uv,
                       dst_stride_uv, tmp_uv_width, tmp_uv_height);
  return true;
}

enum class SupportResult {
  Supported,
  SupportedWithPivot,
  Unsupported,
};

SupportResult IsFormatSupported(Fourcc input_fourcc, Fourcc output_fourcc) {
  static constexpr struct {
    uint32_t input;
    uint32_t output;
    bool need_pivot;
  } kSupportFormatConversionArray[] = {
      // Conversion.
      {Fourcc::AR24, Fourcc::NV12, false},
      {Fourcc::YU12, Fourcc::NV12, false},
      {Fourcc::YV12, Fourcc::NV12, false},
      {Fourcc::AB24, Fourcc::NV12, true},
      {Fourcc::XB24, Fourcc::NV12, true},
      // Scaling or Rotating.
      {Fourcc::NV12, Fourcc::NV12, true},
  };

  const auto single_input_fourcc = input_fourcc.ToSinglePlanar();
  const auto single_output_fourcc = output_fourcc.ToSinglePlanar();
  if (!single_input_fourcc || !single_output_fourcc)
    return SupportResult::Unsupported;

  // Compare fourccs by formatting single planar formats because LibyuvIP can
  // process either single- or multi-planar formats.
  for (const auto& conv : kSupportFormatConversionArray) {
    const auto conv_input_fourcc = Fourcc::FromUint32(conv.input);
    const auto conv_output_fourcc = Fourcc::FromUint32(conv.output);
    if (!conv_input_fourcc || !conv_output_fourcc)
      continue;
    const auto single_conv_input_fourcc = conv_input_fourcc->ToSinglePlanar();
    const auto single_conv_output_fourcc = conv_output_fourcc->ToSinglePlanar();
    if (!single_conv_input_fourcc || !single_conv_output_fourcc)
      continue;

    if (single_input_fourcc == single_conv_input_fourcc &&
        single_output_fourcc == single_conv_output_fourcc) {
      return conv.need_pivot ? SupportResult::SupportedWithPivot
                             : SupportResult::Supported;
    }
  }

  return SupportResult::Unsupported;
}

}  // namespace

// static
std::unique_ptr<ImageProcessorBackend> LibYUVImageProcessorBackend::Create(
    const PortConfig& input_config,
    const PortConfig& output_config,
    const std::vector<OutputMode>& preferred_output_modes,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner) {
  VLOGF(2);

  std::unique_ptr<VideoFrameMapper> input_frame_mapper;
  // LibYUVImageProcessorBackend supports only memory-based video frame for
  // input.
  VideoFrame::StorageType input_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto input_type : input_config.preferred_storage_types) {
    if (input_type == VideoFrame::STORAGE_DMABUFS ||
        input_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
      input_frame_mapper = VideoFrameMapperFactory::CreateMapper(
          input_config.fourcc.ToVideoPixelFormat(), input_type, true);
      if (input_frame_mapper) {
        input_storage_type = input_type;
        break;
      }
    }

    if (VideoFrame::IsStorageTypeMappable(input_type)) {
      input_storage_type = input_type;
      break;
    }
  }
  if (input_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  std::unique_ptr<VideoFrameMapper> output_frame_mapper;
  VideoFrame::StorageType output_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto output_type : output_config.preferred_storage_types) {
    if (output_type == VideoFrame::STORAGE_DMABUFS ||
        output_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
      output_frame_mapper = VideoFrameMapperFactory::CreateMapper(
          output_config.fourcc.ToVideoPixelFormat(), output_type, true);
      if (output_frame_mapper) {
        output_storage_type = output_type;
        break;
      }
    }

    if (VideoFrame::IsStorageTypeMappable(output_type)) {
      output_storage_type = output_type;
      break;
    }
  }
  if (output_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  if (!base::Contains(preferred_output_modes, OutputMode::IMPORT)) {
    VLOGF(2) << "Only support OutputMode::IMPORT";
    return nullptr;
  }

  SupportResult res =
      IsFormatSupported(input_config.fourcc, output_config.fourcc);
  if (res == SupportResult::Unsupported) {
    VLOGF(2) << "Conversion from " << input_config.fourcc.ToString() << " to "
             << output_config.fourcc.ToString() << " is not supported";
    return nullptr;
  }

  if (input_config.fourcc.ToVideoPixelFormat() ==
      output_config.fourcc.ToVideoPixelFormat()) {
    if (output_config.visible_rect.origin() != gfx::Point(0, 0)) {
      VLOGF(2) << "Output visible rectangle is not (0, 0), "
               << "output_config.visible_rect="
               << output_config.visible_rect.ToString();
      return nullptr;
    }
  }

  scoped_refptr<VideoFrame> intermediate_frame;
  if (res == SupportResult::SupportedWithPivot) {
    intermediate_frame = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, input_config.visible_rect.size(),
        gfx::Rect(input_config.visible_rect.size()),
        input_config.visible_rect.size(), base::TimeDelta());
    if (!intermediate_frame) {
      VLOGF(1) << "Failed to create intermediate frame";
      return nullptr;
    }
  }

  auto processor =
      base::WrapUnique<ImageProcessorBackend>(new LibYUVImageProcessorBackend(
          std::move(input_frame_mapper), std::move(output_frame_mapper),
          std::move(intermediate_frame),
          PortConfig(input_config.fourcc, input_config.size,
                     input_config.planes, input_config.visible_rect,
                     {input_storage_type}),
          PortConfig(output_config.fourcc, output_config.size,
                     output_config.planes, output_config.visible_rect,
                     {output_storage_type}),
          OutputMode::IMPORT, relative_rotation, std::move(error_cb),
          std::move(backend_task_runner)));
  VLOGF(2) << "LibYUVImageProcessorBackend created for converting from "
           << input_config.ToString() << " to " << output_config.ToString();
  return processor;
}

LibYUVImageProcessorBackend::LibYUVImageProcessorBackend(
    std::unique_ptr<VideoFrameMapper> input_frame_mapper,
    std::unique_ptr<VideoFrameMapper> output_frame_mapper,
    scoped_refptr<VideoFrame> intermediate_frame,
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            relative_rotation,
                            std::move(error_cb),
                            std::move(backend_task_runner)),
      input_frame_mapper_(std::move(input_frame_mapper)),
      output_frame_mapper_(std::move(output_frame_mapper)),
      intermediate_frame_(std::move(intermediate_frame)) {}

LibYUVImageProcessorBackend::~LibYUVImageProcessorBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
}

void LibYUVImageProcessorBackend::Process(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DVLOGF(4);
  if (input_frame->storage_type() == VideoFrame::STORAGE_DMABUFS ||
      input_frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DCHECK_NE(input_frame_mapper_.get(), nullptr);
    input_frame = input_frame_mapper_->Map(std::move(input_frame));
    if (!input_frame) {
      VLOGF(1) << "Failed to map input VideoFrame";
      error_cb_.Run();
      return;
    }
  }

  // We don't replace |output_frame| with a mapped frame, because |output_frame|
  // is the output of ImageProcessor.
  scoped_refptr<VideoFrame> mapped_frame = output_frame;
  if (output_frame->storage_type() == VideoFrame::STORAGE_DMABUFS ||
      output_frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DCHECK_NE(output_frame_mapper_.get(), nullptr);
    mapped_frame = output_frame_mapper_->Map(output_frame);
    if (!mapped_frame) {
      VLOGF(1) << "Failed to map output VideoFrame";
      error_cb_.Run();
      return;
    }
  }
  int res = DoConversion(input_frame.get(), mapped_frame.get());
  if (res != 0) {
    VLOGF(1) << "libyuv::I420ToNV12 returns non-zero code: " << res;
    error_cb_.Run();
    return;
  }
  output_frame->set_timestamp(input_frame->timestamp());

  std::move(cb).Run(std::move(output_frame));
}

int LibYUVImageProcessorBackend::DoConversion(const VideoFrame* const input,
                                              VideoFrame* const output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

#define Y_U_V_DATA(fr)                                                \
  fr->data(VideoFrame::kYPlane), fr->stride(VideoFrame::kYPlane),     \
      fr->data(VideoFrame::kUPlane), fr->stride(VideoFrame::kUPlane), \
      fr->data(VideoFrame::kVPlane), fr->stride(VideoFrame::kVPlane)

#define Y_V_U_DATA(fr)                                                \
  fr->data(VideoFrame::kYPlane), fr->stride(VideoFrame::kYPlane),     \
      fr->data(VideoFrame::kVPlane), fr->stride(VideoFrame::kVPlane), \
      fr->data(VideoFrame::kUPlane), fr->stride(VideoFrame::kUPlane)

#define Y_UV_DATA(fr)                                             \
  fr->data(VideoFrame::kYPlane), fr->stride(VideoFrame::kYPlane), \
      fr->data(VideoFrame::kUVPlane), fr->stride(VideoFrame::kUVPlane)

#define RGB_DATA(fr) \
  fr->data(VideoFrame::kARGBPlane), fr->stride(VideoFrame::kARGBPlane)

#define LIBYUV_FUNC(func, i, o)                      \
  libyuv::func(i, o, output->visible_rect().width(), \
               output->visible_rect().height())

  if (output->format() == PIXEL_FORMAT_NV12) {
    switch (input->format()) {
      case PIXEL_FORMAT_I420:
        return LIBYUV_FUNC(I420ToNV12, Y_U_V_DATA(input), Y_UV_DATA(output));
      case PIXEL_FORMAT_YV12:
        return LIBYUV_FUNC(I420ToNV12, Y_V_U_DATA(input), Y_UV_DATA(output));

      // RGB conversions. NOTE: Libyuv functions called here are named in
      // little-endian manner.
      case PIXEL_FORMAT_ARGB:
        return LIBYUV_FUNC(ARGBToNV12, RGB_DATA(input), Y_UV_DATA(output));
      case PIXEL_FORMAT_XBGR:
      case PIXEL_FORMAT_ABGR:
        // There is no libyuv function to convert to RGBA to NV12. Therefore, we
        // convert RGBA to I420 tentatively and thereafter convert the tentative
        // one to NV12.
        LIBYUV_FUNC(ABGRToI420, RGB_DATA(input),
                    Y_U_V_DATA(intermediate_frame_));
        return LIBYUV_FUNC(I420ToNV12, Y_U_V_DATA(intermediate_frame_),
                           Y_UV_DATA(output));
      case PIXEL_FORMAT_NV12:
        // Rotation mode.
        if (relative_rotation_ != VIDEO_ROTATION_0) {
          // The size of |tmp_buffer| of NV12Rotate() should be
          // output_visible_rect().GetArea() / 2, which used to store temporary
          // U and V planes for I420 data. Although
          // |intermediate_frame_->data(0)| is much larger than the required
          // size, we use the frame to simplify the code.
          NV12Rotate(intermediate_frame_->data(0),
                     input->visible_data(VideoFrame::kYPlane),
                     input->stride(VideoFrame::kYPlane),
                     input->visible_data(VideoFrame::kUPlane),
                     input->stride(VideoFrame::kUPlane),
                     input->visible_rect().width(),
                     input->visible_rect().height(), Y_UV_DATA(output),
                     output->visible_rect().width(),
                     output->visible_rect().height(), relative_rotation_);
          return 0;
        }

        // Scaling mode.
        libyuv::NV12Scale(input->visible_data(VideoFrame::kYPlane),
                          input->stride(VideoFrame::kYPlane),
                          input->visible_data(VideoFrame::kUVPlane),
                          input->stride(VideoFrame::kUVPlane),
                          input->visible_rect().width(),
                          input->visible_rect().height(),
                          output->visible_data(VideoFrame::kYPlane),
                          output->stride(VideoFrame::kYPlane),
                          output->visible_data(VideoFrame::kUVPlane),
                          output->stride(VideoFrame::kUVPlane),
                          output->visible_rect().width(),
                          output->visible_rect().height(),
                          libyuv::kFilterBilinear);
        return 0;
      default:
        VLOGF(1) << "Unexpected input format: " << input->format();
        return -1;
    }
  }

#undef Y_U_V_DATA
#undef Y_V_U_DATA
#undef Y_UV_DATA
#undef RGB_DATA
#undef LIBYUV_FUNC

  VLOGF(1) << "Unexpected output format: " << output->format();
  return -1;
}

}  // namespace media
