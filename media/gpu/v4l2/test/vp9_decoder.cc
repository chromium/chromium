// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp9_decoder.h"

#include <linux/media/vp9-ctrls.h>
#include <sys/ioctl.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

inline void conditionally_set_flag(
    struct v4l2_ctrl_vp9_frame_decode_params& params,
    bool condition,
    enum v4l2_vp9_frame_flags flag) {
  params.flags |= condition ? flag : 0;
}

Vp9Decoder::Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : ivf_parser_(std::move(ivf_parser)),
      vp9_parser_(
          std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false)),
      v4l2_ioctl_(std::move(v4l2_ioctl)),
      OUTPUT_queue_(std::move(OUTPUT_queue)),
      CAPTURE_queue_(std::move(CAPTURE_queue)) {}

Vp9Decoder::~Vp9Decoder() = default;

// static
std::unique_ptr<Vp9Decoder> Vp9Decoder::Create(
    std::unique_ptr<IvfParser> ivf_parser,
    const media::IvfFileHeader& file_header) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP9_FRAME;

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  const uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  LOG(INFO) << "Ivf file header: " << file_header.width << " x "
            << file_header.height;

  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/1,
      V4L2_MEMORY_MMAP);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kUncompressedFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/2,
      V4L2_MEMORY_MMAP);

  return base::WrapUnique(
      new Vp9Decoder(std::move(ivf_parser), std::move(v4l2_ioctl),
                     std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

bool Vp9Decoder::Initialize() {
  // TODO(stevecho): remove VIDIOC_ENUM_FRAMESIZES ioctl call
  //   after b/193237015 is resolved.
  if (!v4l2_ioctl_->EnumFrameSizes(OUTPUT_queue_->fourcc()))
    LOG(ERROR) << "EnumFrameSizes for OUTPUT queue failed.";

  if (!v4l2_ioctl_->SetFmt(OUTPUT_queue_))
    LOG(ERROR) << "SetFmt for OUTPUT queue failed.";

  gfx::Size coded_size;
  uint32_t num_planes;
  if (!v4l2_ioctl_->GetFmt(CAPTURE_queue_->type(), &coded_size, &num_planes))
    LOG(ERROR) << "GetFmt for CAPTURE queue failed.";

  CAPTURE_queue_->set_coded_size(coded_size);
  CAPTURE_queue_->set_num_planes(num_planes);

  // VIDIOC_TRY_FMT() ioctl is equivalent to VIDIOC_S_FMT
  // with one exception that it does not change driver state.
  // VIDIOC_TRY_FMT may or may not be needed; it's used by the stateful
  // Chromium V4L2VideoDecoder backend, see b/190733055#comment78.
  // TODO(b/190733055): try and remove it after landing all the code.
  if (!v4l2_ioctl_->TryFmt(CAPTURE_queue_))
    LOG(ERROR) << "TryFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->SetFmt(CAPTURE_queue_))
    LOG(ERROR) << "SetFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->ReqBufs(OUTPUT_queue_))
    LOG(ERROR) << "ReqBufs for OUTPUT queue failed.";

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_))
    LOG(ERROR) << "QueryAndMmapQueueBuffers for OUTPUT queue failed";

  if (!v4l2_ioctl_->ReqBufs(CAPTURE_queue_))
    LOG(ERROR) << "ReqBufs for CAPTURE queue failed.";

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_))
    LOG(ERROR) << "QueryAndMmapQueueBuffers for CAPTURE queue failed.";

  for (uint32_t i = 0; i < kRequestBufferCount; ++i) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, i))
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";
  }

  if (!v4l2_ioctl_->MediaIocRequestAlloc())
    LOG(ERROR) << "MEDIA_IOC_REQUEST_ALLOC failed";

  if (!v4l2_ioctl_->StreamOn(OUTPUT_queue_->type()))
    LOG(ERROR) << "StreamOn for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOn(CAPTURE_queue_->type()))
    LOG(ERROR) << "StreamOn for CAPTURE queue failed.";

  return true;
}

Vp9Parser::Result Vp9Decoder::ReadNextFrame(Vp9FrameHeader& vp9_frame_header,
                                            gfx::Size& size) {
  // TODO(jchinlee): reexamine this loop for cleanup.
  while (true) {
    std::unique_ptr<DecryptConfig> null_config;
    Vp9Parser::Result res =
        vp9_parser_->ParseNextFrame(&vp9_frame_header, &size, &null_config);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header{};
      const uint8_t* ivf_frame_data;

      if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
        return Vp9Parser::kEOStream;

      vp9_parser_->SetStream(ivf_frame_data, ivf_frame_header.frame_size,
                             /*stream_config=*/nullptr);
      continue;
    }

    return res;
  }
}

Vp9Decoder::Result Vp9Decoder::DecodeNextFrame() {
  gfx::Size size;
  Vp9FrameHeader frame_hdr{};

  Vp9Parser::Result parser_res = ReadNextFrame(frame_hdr, size);
  switch (parser_res) {
    case Vp9Parser::kInvalidStream:
      LOG_ASSERT(false) << "Failed to parse frame";
      return Vp9Decoder::kError;
    case Vp9Parser::kAwaitingRefresh:
      LOG_ASSERT(false) << "Unsupported parser return value";
      return Vp9Decoder::kError;
    case Vp9Parser::kEOStream:
      return Vp9Decoder::kEOStream;
    case Vp9Parser::kOk:
      break;
  }

  struct v4l2_ctrl_vp9_frame_decode_params v4l2_frame_params;
  memset(&v4l2_frame_params, 0, sizeof(v4l2_frame_params));

  conditionally_set_flag(v4l2_frame_params,
                         frame_hdr.frame_type == Vp9FrameHeader::KEYFRAME,
                         V4L2_VP9_FRAME_FLAG_KEY_FRAME);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.show_frame,
                         V4L2_VP9_FRAME_FLAG_SHOW_FRAME);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.error_resilient_mode,
                         V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.intra_only,
                         V4L2_VP9_FRAME_FLAG_INTRA_ONLY);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.allow_high_precision_mv,
                         V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.refresh_frame_context,
                         V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX);
  conditionally_set_flag(v4l2_frame_params,
                         frame_hdr.frame_parallel_decoding_mode,
                         V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.subsampling_x,
                         V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.subsampling_y,
                         V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING);
  conditionally_set_flag(v4l2_frame_params, frame_hdr.color_range,
                         V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING);

  v4l2_frame_params.compressed_header_size = frame_hdr.header_size_in_bytes;
  v4l2_frame_params.uncompressed_header_size =
      frame_hdr.uncompressed_header_size;
  v4l2_frame_params.profile = frame_hdr.profile;
  // As per the VP9 specification:
  switch (frame_hdr.reset_frame_context) {
    // "0 or 1 implies don’t reset."
    case 0:
    case 1:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
    // "2 resets just the context specified in the frame header."
    case 2:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_SPEC;
      break;
    // "3 reset all contexts."
    case 3:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_ALL;
      break;
    default:
      LOG(FATAL) << "Invalid reset frame context value!";
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
  }
  v4l2_frame_params.frame_context_idx =
      frame_hdr.frame_context_idx_to_save_probs;
  v4l2_frame_params.bit_depth = frame_hdr.bit_depth;

  v4l2_frame_params.interpolation_filter = frame_hdr.interpolation_filter;
  v4l2_frame_params.tile_cols_log2 = frame_hdr.tile_cols_log2;
  v4l2_frame_params.tile_rows_log2 = frame_hdr.tile_rows_log2;
  v4l2_frame_params.tx_mode = frame_hdr.compressed_header.tx_mode;
  v4l2_frame_params.reference_mode = frame_hdr.compressed_header.reference_mode;
  static_assert(VP9_FRAME_LAST + (V4L2_REF_ID_CNT - 1) <
                    std::extent<decltype(frame_hdr.ref_frame_sign_bias)>::value,
                "array sizes are incompatible");
  for (size_t i = 0; i < V4L2_REF_ID_CNT; i++) {
    v4l2_frame_params.ref_frame_sign_biases |=
        (frame_hdr.ref_frame_sign_bias[i + VP9_FRAME_LAST] ? (1 << i) : 0);
  }
  v4l2_frame_params.frame_width_minus_1 = frame_hdr.frame_width - 1;
  v4l2_frame_params.frame_height_minus_1 = frame_hdr.frame_height - 1;
  v4l2_frame_params.render_width_minus_1 = frame_hdr.render_width - 1;
  v4l2_frame_params.render_height_minus_1 = frame_hdr.render_height - 1;

  // TODO(stevecho): fill in the rest of |v4l2_frame_params| fields.

  return Vp9Decoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
