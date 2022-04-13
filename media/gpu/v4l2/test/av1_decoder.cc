// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/av1_decoder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/filters/ivf_parser.h"

namespace media {

namespace v4l2_test {

Av1Decoder::Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(ivf_parser),
                                 std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      buffer_pool_(std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr)),
      state_(std::make_unique<libgav1::DecoderState>()) {}

Av1Decoder::~Av1Decoder() {
  // We destroy the state explicitly to ensure it's destroyed before the
  // |buffer_pool_|. The |buffer_pool_| checks that all the allocated frames
  // are released in its destructor.
  state_.reset();
  DCHECK(buffer_pool_);
}

// static
std::unique_ptr<Av1Decoder> Av1Decoder::Create(
    std::unique_ptr<IvfParser> ivf_parser,
    const media::IvfFileHeader& file_header) {
  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_AV1_FRAME;

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  constexpr uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  // TODO(stevecho): this might need some driver patches to support AV1F
  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  LOG(INFO) << "Ivf file header: " << file_header.width << " x "
            << file_header.height;

  // TODO(stevecho): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/1,
      V4L2_MEMORY_MMAP, /*num_buffers=*/1);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.16/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kUncompressedFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/2,
      V4L2_MEMORY_MMAP, /*num_buffers=*/10);

  return base::WrapUnique(
      new Av1Decoder(std::move(ivf_parser), std::move(v4l2_ioctl),
                     std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

Av1Decoder::ParsingResult Av1Decoder::ReadNextFrame(
    libgav1::RefCountedBufferPtr& current_frame) {
  if (!obu_parser_ || !obu_parser_->HasData()) {
    if (!ivf_parser_->ParseNextFrame(&ivf_frame_header_, &ivf_frame_data_))
      return ParsingResult::kEOStream;

    // The ObuParser has run out of data or did not exist in the first place. It
    // has no "replace the current buffer with a new buffer of a different size"
    // method; we must make a new parser.
    // (std::nothrow) is required for the base class Allocable of
    // libgav1::ObuParser
    obu_parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        ivf_frame_data_, ivf_frame_header_.frame_size, /*operating_point=*/0,
        buffer_pool_.get(), state_.get()));
    if (current_sequence_header_)
      obu_parser_->set_sequence_header(*current_sequence_header_);
  }

  const libgav1::StatusCode code = obu_parser_->ParseOneFrame(&current_frame);
  if (code != libgav1::kStatusOk) {
    LOG(ERROR) << "Error parsing OBU stream: " << libgav1::GetErrorString(code);
    return ParsingResult::kFailed;
  }
  return ParsingResult::kOk;
}

VideoDecoder::Result Av1Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                 std::vector<char>& u_plane,
                                                 std::vector<char>& v_plane,
                                                 gfx::Size& size,
                                                 const int frame_number) {
  libgav1::RefCountedBufferPtr current_frame;
  const ParsingResult parser_res = ReadNextFrame(current_frame);

  if (parser_res != ParsingResult::kOk) {
    LOG_ASSERT(parser_res == ParsingResult::kEOStream)
        << "Failed to parse next frame.";
    return VideoDecoder::kEOStream;
  }

  if (obu_parser_->sequence_header_changed())
    current_sequence_header_.emplace(obu_parser_->sequence_header());

  LOG_ASSERT(current_sequence_header_)
      << "Sequence header missing for decoding.";

  // TODO(b/228534725): add changes to support reference frames management

  // TODO(b/228534730): add changes to prepare parameters for V4L2 AV1 stateless
  // decoding

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
