// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp8_decoder.h"

// ChromeOS specific header; does not exist upstream
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/vp8-ctrls-upstream.h>
#endif

#include <linux/v4l2-controls.h>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/parsers/vp8_parser.h"

namespace media {
namespace v4l2_test {

// TODO(b/256252128): Find optimal number of CAPTURE buffers
constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;
constexpr uint32_t kNumberOfBuffersInOutputQueue = 1;

constexpr uint32_t kOutputQueueBufferIndex = 0;

constexpr uint32_t kNumberOfPlanesInOutputQueue = 1;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

static_assert(kNumberOfBuffersInOutputQueue == 1,
              "Too many buffers in OUTPUT queue. It is currently designed to "
              "support only 1 request at a time.");

static_assert(kNumberOfPlanesInOutputQueue == 1,
              "Number of planes is expected to be 1 for OUTPUT queue.");

Vp8Decoder::Vp8Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      ivf_parser_(std::move(ivf_parser)),
      vp8_parser_(std::make_unique<Vp8Parser>()) {
  DCHECK(v4l2_ioctl_);
  DCHECK(v4l2_ioctl_->QueryCtrl(V4L2_CID_STATELESS_VP8_FRAME));
}

Vp8Decoder::~Vp8Decoder() = default;

// static
std::unique_ptr<Vp8Decoder> Vp8Decoder::Create(
    const base::MemoryMappedFile& stream) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP8_FRAME;

  VLOG(2) << "Attempting to create decoder with codec "
          << media::FourccToString(kDriverCodecFourcc);

  // Set up video parser.
  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};

  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  const auto driver_codec_fourcc =
      media::v4l2_test::FileFourccToDriverFourcc(file_header.fourcc);

  if (driver_codec_fourcc != kDriverCodecFourcc) {
    VLOG(2) << "File fourcc (" << media::FourccToString(driver_codec_fourcc)
            << ") does not match expected fourcc("
            << media::FourccToString(kDriverCodecFourcc) << ").";
    return nullptr;
  }

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  // TODO(b/256174196): Extend decoder format options to support non-MTK devices
  const uint32_t kMm21Fourcc = v4l2_fourcc('M', 'M', '2', '1');

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc, kMm21Fourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  LOG(INFO) << "Ivf file header: " << file_header.width << " x "
            << file_header.height;

  // TODO(b/256251694): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/1,
      V4L2_MEMORY_MMAP, /*num_buffers=*/kNumberOfBuffersInOutputQueue);

  // TODO(b/256543928): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kMm21Fourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/2,
      V4L2_MEMORY_MMAP, /*num_buffers=*/kNumberOfBuffersInCaptureQueue);

  return base::WrapUnique(
      new Vp8Decoder(std::move(ivf_parser), std::move(v4l2_ioctl),
                     std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

Vp8Decoder::ParseResult Vp8Decoder::ReadNextFrame(
    Vp8FrameHeader& vp8_frame_header) {
  IvfFrameHeader ivf_frame_header{};
  const uint8_t* ivf_frame_data;
  if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
    return kEOStream;

  const bool result = vp8_parser_->ParseFrame(
      ivf_frame_data, ivf_frame_header.frame_size, &vp8_frame_header);

  return result ? Vp8Decoder::kOk : Vp8Decoder::kError;
}

VideoDecoder::Result Vp8Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                 std::vector<char>& u_plane,
                                                 std::vector<char>& v_plane,
                                                 gfx::Size& size,
                                                 const int frame_number) {
  Vp8FrameHeader frame_hdr{};

  Vp8Decoder::ParseResult parser_res = ReadNextFrame(frame_hdr);
  switch (parser_res) {
    case Vp8Decoder::kEOStream:
      return VideoDecoder::kEOStream;
    case Vp8Decoder::kError:
      return VideoDecoder::kError;
    case Vp8Decoder::kOk:
      break;
  }

  // Copies the frame data into the V4L2 buffer of OUTPUT |queue|.
  scoped_refptr<MmapedBuffer> OUTPUT_queue_buffer =
      OUTPUT_queue_->GetBuffer(kOutputQueueBufferIndex);
  OUTPUT_queue_buffer->mmaped_planes()[0].CopyIn(frame_hdr.data,
                                                 frame_hdr.frame_size);
  OUTPUT_queue_buffer->set_frame_number(frame_number);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for OUTPUT queue.";

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
