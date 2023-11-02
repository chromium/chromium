// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_decoder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace media {

namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

// static
std::unique_ptr<H264Decoder> H264Decoder::Create(
    const base::MemoryMappedFile& stream) {
  auto parser = std::make_unique<H264Parser>();
  parser->SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an h.264 bistreams starts with an SPS.
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser->AdvanceToNextNALU(&nalu);
    if (res != H264Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H264NALU::kSPS)
      break;
  }

  int id;
  H264Parser::Result res = parser->ParseSPS(&id);
  CHECK(res == H264Parser::kOk);

  const H264SPS* sps = parser->GetSPS(id);
  CHECK(sps);

  absl::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);
  LOG(INFO) << "h.264 coded size : " << coded_size->ToString();

  // Currently only MM21 as an uncompressed format is supported
  // because it can be converted by libyuv to NV12, which is then
  // used to verify via MD5sum.
  constexpr uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_H264_SLICE;

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the required FourCCs.";
    return nullptr;
  }

  // TODO(stevecho): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc, coded_size.value(),
      /*num_planes=*/1, V4L2_MEMORY_MMAP, /*num_buffers=*/1);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kUncompressedFourcc,
      coded_size.value(),
      /*num_planes=*/2, V4L2_MEMORY_MMAP,
      /*num_buffers=*/kNumberOfBuffersInCaptureQueue);

  return base::WrapUnique(
      new H264Decoder(std::move(parser), std::move(v4l2_ioctl),
                      std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

H264Decoder::H264Decoder(std::unique_ptr<H264Parser> parser,
                         std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         std::unique_ptr<V4L2Queue> OUTPUT_queue,
                         std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      parser_(std::move(parser)) {}

H264Decoder::~H264Decoder() = default;

VideoDecoder::Result H264Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                  std::vector<char>& u_plane,
                                                  std::vector<char>& v_plane,
                                                  gfx::Size& size,
                                                  const int frame_number) {
  H264NALU nalu;
  H264Parser::Result res = parser_->AdvanceToNextNALU(&nalu);
  if (res != H264Parser::kOk)
    return VideoDecoder::kError;

  LOG(INFO) << "NALU " << nalu.nal_unit_type;

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
