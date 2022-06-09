// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/av1_decoder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/v4l2/test/av1_pix_fmt.h"

namespace media {

namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

Av1Decoder::Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      ivf_parser_(std::move(ivf_parser)),
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
    const base::MemoryMappedFile& stream) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_AV1_FRAME;

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

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

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

void Av1Decoder::CopyFrameData(const libgav1::ObuFrameHeader& frame_hdr,
                               std::unique_ptr<V4L2Queue>& queue) {
  CHECK_EQ(queue->num_buffers(), 1u)
      << "Only 1 buffer is expected to be used for OUTPUT queue for now.";

  CHECK_EQ(queue->num_planes(), 1u)
      << "Number of planes is expected to be 1 for OUTPUT queue.";

  scoped_refptr<MmapedBuffer> buffer = queue->GetBuffer(0);

  memcpy(static_cast<uint8_t*>(buffer->mmaped_planes()[0].start_addr),
         ivf_frame_data_, ivf_frame_header_.frame_size);
}

std::set<int> Av1Decoder::RefreshReferenceSlots(
    uint8_t refresh_frame_flags,
    libgav1::RefCountedBufferPtr current_frame,
    scoped_refptr<MmapedBuffer> buffer,
    uint32_t last_queued_buffer_index) {
  static_assert(
      kAv1NumRefFrames == sizeof(refresh_frame_flags) * CHAR_BIT,
      "|refresh_frame_flags| size must be equal to |kAv1NumRefFrames|");

  const std::bitset<kAv1NumRefFrames> refresh_frame_slots(refresh_frame_flags);

  std::set<int> reusable_buffer_ids;

  constexpr uint8_t kRefreshFrameFlagsNone = 0;
  if (refresh_frame_flags == kRefreshFrameFlagsNone) {
    // Indicates to reuse currently decoded CAPTURE buffer.
    reusable_buffer_ids.insert(buffer->buffer_id());

    return reusable_buffer_ids;
  }

  constexpr uint8_t kRefreshFrameFlagsAll = 0xFF;
  if (refresh_frame_flags == kRefreshFrameFlagsAll) {
    // After decoding a key frame, all CAPTURE buffers can be reused except the
    // CAPTURE buffer corresponding to the key frame.
    for (size_t i = 0; i < kNumberOfBuffersInCaptureQueue; i++)
      reusable_buffer_ids.insert(i);

    reusable_buffer_ids.erase(buffer->buffer_id());

    // Note that the CAPTURE buffer for previous frame can be used as well,
    // but it is already queued again at this point.
    reusable_buffer_ids.erase(last_queued_buffer_index);

    // Updates to assign current key frame as a reference frame for all
    // reference frame slots in the reference frames list.
    ref_frames_.fill(buffer);

    return reusable_buffer_ids;
  }

  // More than one slot in |refresh_frame_flags| can be set.
  for (size_t i = 0; i < kAv1NumRefFrames; i++) {
    if (!refresh_frame_slots[i])
      continue;

    // It is not required to check whether existing reference frame slot is
    // already pointing to a reference frame. This is because reference
    // frame slots are empty only after the first key frame decoding.
    const uint16_t reusable_candidate_buffer_id = ref_frames_[i]->buffer_id();
    reusable_buffer_ids.insert(reusable_candidate_buffer_id);

    // Checks to make sure |reusable_candidate_buffer_id| is not used in
    // different reference frame slots in the reference frames list. If
    // |reusable_candidate_buffer_id| is already being used, then it is no
    // longer qualified as a reusable buffer. Thus, it is removed from
    // |reusable_buffer_ids|.
    for (size_t j = 0; j < kAv1NumRefFrames; j++) {
      const bool is_refresh_slot_not_used = (refresh_frame_slots[j] == false);
      const bool is_candidate_used =
          (ref_frames_[j]->buffer_id() == reusable_candidate_buffer_id);

      if (is_refresh_slot_not_used && is_candidate_used) {
        reusable_buffer_ids.erase(reusable_candidate_buffer_id);
        break;
      }
    }
    ref_frames_[i] = buffer;
  }

  state_->UpdateReferenceFrames(current_frame,
                                base::strict_cast<int>(refresh_frame_flags));

  return reusable_buffer_ids;
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

  libgav1::ObuFrameHeader current_frame_header = obu_parser_->frame_header();

  if (obu_parser_->sequence_header_changed())
    current_sequence_header_.emplace(obu_parser_->sequence_header());

  LOG_ASSERT(current_sequence_header_)
      << "Sequence header missing for decoding.";

  CopyFrameData(current_frame_header, OUTPUT_queue_);

  LOG_ASSERT(OUTPUT_queue_->num_buffers() == 1)
      << "Too many buffers in OUTPUT queue. It is currently designed to "
         "support only 1 request at a time.";

  OUTPUT_queue_->GetBuffer(0)->set_frame_number(frame_number);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for OUTPUT queue.";

  // TODO(b/230891887): use uint64_t when v4l2_timeval_to_ns() function is used.
  constexpr uint32_t kInvalidSurface = std::numeric_limits<uint32_t>::max();

  for (const auto ref_frame_index :
       current_frame_header.reference_frame_index) {
    LOG_ASSERT(ref_frame_index < kAv1NumRefFrames)
        << "Invalid reference frame index.\n";

    constexpr size_t kTimestampToNanoSecs = 1000;

    // |reference_id| is needed to use previously decoded frames
    // from reference frames list.
    const auto reference_id =
        ref_frames_[ref_frame_index]
            ? ref_frames_[ref_frame_index]->frame_number() *
                  kTimestampToNanoSecs
            : kInvalidSurface;

    // TODO(stevecho): add setup for frame parameters using |reference_id|
    // when av1 kernel header is ready.
    ANALYZER_ALLOW_UNUSED(reference_id);
  }

  // TODO(b/228534730): add changes to prepare parameters for V4L2 AV1 stateless
  // decoding

  if (!v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_))
    LOG(FATAL) << "MEDIA_REQUEST_IOC_QUEUE failed.";

  uint32_t index;

  if (!v4l2_ioctl_->DQBuf(CAPTURE_queue_, &index))
    LOG(FATAL) << "VIDIOC_DQBUF failed for CAPTURE queue.";

  scoped_refptr<MmapedBuffer> buffer = CAPTURE_queue_->GetBuffer(index);

  if (!v4l2_ioctl_->DQBuf(OUTPUT_queue_, &index))
    LOG(FATAL) << "VIDIOC_DQBUF failed for OUTPUT queue.";

  if (!v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_))
    LOG(FATAL) << "MEDIA_REQUEST_IOC_REINIT failed.";

  const std::set<int> reusable_buffer_ids =
      RefreshReferenceSlots(current_frame_header.refresh_frame_flags,
                            current_frame, CAPTURE_queue_->GetBuffer(index),
                            CAPTURE_queue_->last_queued_buffer_index());

  for (const auto reusable_buffer_id : reusable_buffer_ids) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_id))
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";

    if (!libgav1::IsIntraFrame(current_frame_header.frame_type))
      CAPTURE_queue_->set_last_queued_buffer_index(reusable_buffer_id);
  }

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
