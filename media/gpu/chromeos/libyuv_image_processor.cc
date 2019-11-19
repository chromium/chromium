// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/libyuv_image_processor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

namespace media {

namespace {

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
      {Fourcc::AR24, Fourcc::NV12, false}, {Fourcc::YU12, Fourcc::NV12, false},
      {Fourcc::YV12, Fourcc::NV12, false}, {Fourcc::AB24, Fourcc::NV12, true},
      {Fourcc::XB24, Fourcc::NV12, true},
  };

  for (const auto& conv : kSupportFormatConversionArray) {
    if (conv.input == input_fourcc && conv.output == output_fourcc) {
      return conv.need_pivot ? SupportResult::SupportedWithPivot
                             : SupportResult::Supported;
    }
  }

  return SupportResult::Unsupported;
}

}  // namespace

LibYUVImageProcessor::LibYUVImageProcessor(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    std::unique_ptr<VideoFrameMapper> video_frame_mapper,
    ErrorCB error_cb)
    : ImageProcessor(input_config, output_config, OutputMode::IMPORT),
      video_frame_mapper_(std::move(video_frame_mapper)),
      error_cb_(error_cb),
      process_thread_("LibYUVImageProcessorThread") {}

LibYUVImageProcessor::~LibYUVImageProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  Reset();

  process_thread_.Stop();
}

// static
std::unique_ptr<LibYUVImageProcessor> LibYUVImageProcessor::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    ImageProcessor::OutputMode output_mode,
    ErrorCB error_cb) {
  VLOGF(2);

  std::unique_ptr<VideoFrameMapper> video_frame_mapper;
  // LibYUVImageProcessor supports only memory-based video frame for input.
  VideoFrame::StorageType input_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto input_type : input_config.preferred_storage_types) {
#if defined(OS_LINUX)
    if (input_type == VideoFrame::STORAGE_DMABUFS) {
      video_frame_mapper = VideoFrameMapperFactory::CreateMapper(
          input_config.fourcc.ToVideoPixelFormat(), VideoFrame::STORAGE_DMABUFS,
          true);
      if (video_frame_mapper) {
        input_storage_type = input_type;
        break;
      }
    }
#endif  // defined(OS_LINUX)

    if (VideoFrame::IsStorageTypeMappable(input_type)) {
      input_storage_type = input_type;
      break;
    }
  }
  if (input_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  // LibYUVImageProcessor supports only memory-based video frame for output.
  VideoFrame::StorageType output_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto output_type : output_config.preferred_storage_types) {
    if (VideoFrame::IsStorageTypeMappable(output_type)) {
      output_storage_type = output_type;
      break;
    }
  }
  if (output_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  if (output_mode != OutputMode::IMPORT) {
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

  auto processor = base::WrapUnique(new LibYUVImageProcessor(
      ImageProcessor::PortConfig(input_config.fourcc, input_config.size,
                                 input_config.planes, input_config.visible_size,
                                 {input_storage_type}),
      ImageProcessor::PortConfig(
          output_config.fourcc, output_config.size, output_config.planes,
          output_config.visible_size, {output_storage_type}),
      std::move(video_frame_mapper),
      media::BindToCurrentLoop(std::move(error_cb))));
  if (res == SupportResult::SupportedWithPivot) {
    processor->intermediate_frame_ =
        VideoFrame::CreateFrame(PIXEL_FORMAT_I420, input_config.visible_size,
                                gfx::Rect(input_config.visible_size),
                                input_config.visible_size, base::TimeDelta());
    if (!processor->intermediate_frame_) {
      VLOGF(1) << "Failed to create intermediate frame";
      return nullptr;
    }
  }

  if (!processor->process_thread_.Start()) {
    VLOGF(1) << "Failed to start processing thread";
    return nullptr;
  }

  VLOGF(2) << "LibYUVImageProcessor created for converting from "
           << input_config.ToString() << " to " << output_config.ToString();
  return processor;
}

bool LibYUVImageProcessor::ProcessInternal(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DVLOGF(4);
  DCHECK_EQ(input_frame->layout().format(),
            input_config_.fourcc.ToVideoPixelFormat());
  DCHECK(input_frame->layout().coded_size() == input_config_.size);
  DCHECK_EQ(output_frame->layout().format(),
            output_config_.fourcc.ToVideoPixelFormat());
  DCHECK(output_frame->layout().coded_size() == output_config_.size);
  DCHECK(input_config_.storage_type() == input_frame->storage_type() ||
         VideoFrame::IsStorageTypeMappable(input_frame->storage_type()));
  DCHECK(VideoFrame::IsStorageTypeMappable(output_frame->storage_type()));

  // Since process_thread_ is owned by this class. base::Unretained(this) and
  // the raw pointer of that task runner are safe.
  process_task_tracker_.PostTask(
      process_thread_.task_runner().get(), FROM_HERE,
      base::BindOnce(&LibYUVImageProcessor::ProcessTask, base::Unretained(this),
                     std::move(input_frame), std::move(output_frame),
                     std::move(cb)));
  return true;
}

void LibYUVImageProcessor::ProcessTask(scoped_refptr<VideoFrame> input_frame,
                                       scoped_refptr<VideoFrame> output_frame,
                                       FrameReadyCB cb) {
  DCHECK(process_thread_.task_runner()->BelongsToCurrentThread());
  DVLOGF(4);
#if defined(OS_LINUX)
  if (input_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    DCHECK_NE(video_frame_mapper_.get(), nullptr);
    input_frame = video_frame_mapper_->Map(std::move(input_frame));
    if (!input_frame) {
      VLOGF(1) << "Failed to map input VideoFrame";
      NotifyError();
      return;
    }
  }
#endif  // defined(OS_LINUX)

  int res = DoConversion(input_frame.get(), output_frame.get());
  if (res != 0) {
    VLOGF(1) << "libyuv::I420ToNV12 returns non-zero code: " << res;
    NotifyError();
    return;
  }
  output_frame->set_timestamp(input_frame->timestamp());
  std::move(cb).Run(std::move(output_frame));
}

bool LibYUVImageProcessor::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  process_task_tracker_.TryCancelAll();
  return true;
}

void LibYUVImageProcessor::NotifyError() {
  VLOGF(1);
  error_cb_.Run();
}

int LibYUVImageProcessor::DoConversion(const VideoFrame* const input,
                                       VideoFrame* const output) {
  DCHECK(process_thread_.task_runner()->BelongsToCurrentThread());

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
