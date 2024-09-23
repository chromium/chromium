// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_test_helpers.h"

#include <limits>
#include <numeric>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/byte_conversions.h"
#include "base/stl_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/format_utils.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/test/raw_video.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {
namespace {
constexpr uint16_t kIvfFileHeaderSize = 32;
constexpr size_t kIvfFrameHeaderSize = 12;

constexpr size_t kNALUHeaderSize = 4;
constexpr size_t kNALUReducedHeaderSize = 3;

// If |reverse| is true , GetNextFrame() for a frame returns frames in a
// round-trip playback fashion (0, 1,.., |num_frames| - 2, |num_frames| - 1,
// |num_frames| - 2, |num_frames_| - 3,.., 1, 0, 1, 2,..).
// If |reverse| is false, GetNextFrame() just loops the stream (0, 1,..,
// |num_frames| - 2, |num_frames| - 1, 0, 1,..).
uint32_t GetReadFrameIndex(uint32_t frame_index,
                           bool reverse,
                           uint32_t num_frames) {
  CHECK_GT(num_frames, 1u);
  if (!reverse)
    return frame_index % num_frames;
  // 0, .., num_frames - 1, num_frames - 2
  // 0-num_frame, num_frames - 1, ... 1, 0, 1, nu
  const size_t num_frames_in_loop = num_frames + num_frames - 2;
  frame_index = frame_index % num_frames_in_loop;
  if (frame_index < num_frames) {
    return frame_index;
  }
  frame_index -= num_frames;
  return num_frames - 2 - frame_index;
}
}  // namespace

IvfFileHeader GetIvfFileHeader(base::span<const uint8_t> data) {
  LOG_ASSERT(data.size_bytes() == 32u);
  IvfFileHeader file_header;
  base::byte_span_from_ref(file_header).copy_from(data.first<32u>());
  return file_header;
}

IvfFrameHeader GetIvfFrameHeader(base::span<const uint8_t> data) {
  LOG_ASSERT(data.size_bytes() == 12u);
  IvfFrameHeader frame_header;
  auto [frame_size, timestamp] = data.first<12u>().split_at<4u>();
  frame_header.frame_size = base::U32FromLittleEndian(frame_size);
  frame_header.timestamp = base::U64FromLittleEndian(timestamp);
  return frame_header;
}

IvfWriter::IvfWriter(base::FilePath output_filepath) {
  output_file_ = base::File(
      output_filepath, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  LOG_ASSERT(output_file_.IsValid());
}

bool IvfWriter::WriteFileHeader(VideoCodec codec,
                                const gfx::Size& resolution,
                                uint32_t frame_rate,
                                uint32_t num_frames) {
  constexpr uint16_t kVersion = 0;

  char ivf_header[kIvfFileHeaderSize] = {};
  auto writer = base::SpanWriter(base::as_writable_byte_span(ivf_header));
  // Bytes 0-3 of an IVF file header always contain the signature 'DKIF'.
  writer.Write(base::as_byte_span({'D', 'K', 'I', 'F'}));

  writer.WriteU16LittleEndian(kVersion);
  writer.WriteU16LittleEndian(kIvfFileHeaderSize);
  switch (codec) {
    case VideoCodec::kVP8:
      writer.Write(base::as_byte_span({'V', 'P', '8', '0'}));
      break;
    case VideoCodec::kVP9:
      writer.Write(base::as_byte_span({'V', 'P', '9', '0'}));
      break;
    case VideoCodec::kAV1:
      writer.Write(base::as_byte_span({'A', 'V', '0', '1'}));
      break;
    default:
      LOG(ERROR) << "Unknown codec: " << GetCodecName(codec);
      return false;
  }

  writer.WriteU16LittleEndian(resolution.width());
  writer.WriteU16LittleEndian(resolution.height());
  writer.WriteU32LittleEndian(frame_rate);
  writer.WriteU32LittleEndian(1u);
  writer.WriteU32LittleEndian(num_frames);
  // Reserved.
  writer.WriteU32LittleEndian(0u);
  CHECK_EQ(writer.remaining(), 0u);

  return output_file_.WriteAtCurrentPosAndCheck(base::as_byte_span(ivf_header));
}

bool IvfWriter::WriteFrame(uint32_t data_size,
                           uint64_t timestamp,
                           const uint8_t* data) {
  char ivf_frame_header[kIvfFrameHeaderSize] = {};
  memcpy(&ivf_frame_header[0], &data_size, sizeof(data_size));
  memcpy(&ivf_frame_header[4], &timestamp, sizeof(timestamp));
  if (!output_file_.WriteAtCurrentPosAndCheck(
          base::as_byte_span(ivf_frame_header))) {
    return false;
  }
  auto data_span = UNSAFE_TODO(base::span(data, data_size));
  return output_file_.WriteAtCurrentPosAndCheck(data_span);
}

// static
std::unique_ptr<EncodedDataHelper> EncodedDataHelper::Create(
    base::span<const uint8_t> stream,
    VideoCodec codec) {
  if (codec == VideoCodec::kH264) {
    return std::make_unique<EncodedDataHelperH26x>(std::move(stream), codec);
  }
  if (codec == VideoCodec::kHEVC) {
    // Depending on ENABLE_HEVC_PARSER_AND_HW_DECODER, use a sophisticated H265
    // parser or the same NALU hunter as EncodedDataHelperH26x.
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    return std::make_unique<EncodedDataHelperH265>(std::move(stream), codec);
#else
    return std::make_unique<EncodedDataHelperH26x>(std::move(stream), codec);
#endif
  }
  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9 ||
      codec == VideoCodec::kAV1) {
    return std::make_unique<EncodedDataHelperIVF>(std::move(stream), codec);
  }
  NOTREACHED() << "Unsupported codec " << GetCodecName(codec);
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodec codec) {
  CHECK(codec == media::VideoCodec::kH264 || codec == media::VideoCodec::kHEVC)
      << "Unsupported codec " << GetCodecName(codec);
  return EncodedDataHelperH26x::HasConfigInfo(data, size, codec);
}

EncodedDataHelper::EncodedDataHelper(base::span<const uint8_t> stream,
                                     VideoCodec codec)
    : data_(std::string(reinterpret_cast<const char*>(stream.data()),
                        stream.size())),
      codec_(codec) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

void EncodedDataHelper::Rewind() {
  next_pos_to_parse_ = 0;
}

bool EncodedDataHelper::ReachEndOfStream() const {
  return next_pos_to_parse_ == data_.size();
}

EncodedDataHelperH26x::EncodedDataHelperH26x(base::span<const uint8_t> stream,
                                             VideoCodec codec)
    : EncodedDataHelper(std::move(stream), codec) {}

// static
bool EncodedDataHelperH26x::HasConfigInfo(const uint8_t* data,
                                          size_t size,
                                          VideoCodec codec) {
  // Check if this is an H264 SPS NALU w/ a kNALUReducedHeaderSize or
  // kNALUHeaderSize byte start code.
  if (codec == media::VideoCodec::kH264) {
    return (size > kNALUReducedHeaderSize && data[0] == 0x0 && data[1] == 0x0 &&
            data[2] == 0x1 && (data[kNALUReducedHeaderSize] & 0x1f) == 0x7) ||
           (size > kNALUHeaderSize && data[0] == 0x0 && data[1] == 0x0 &&
            data[2] == 0x0 && data[3] == 0x1 &&
            (data[kNALUHeaderSize] & 0x1f) == 0x7);
  }
  CHECK_EQ(codec, media::VideoCodec::kHEVC);
  return (size > kNALUReducedHeaderSize && data[0] == 0x0 && data[1] == 0x0 &&
          data[2] == 0x1 && (data[kNALUReducedHeaderSize] & 0x7e) == 0x42) ||
         (size > kNALUHeaderSize && data[0] == 0x0 && data[1] == 0x0 &&
          data[2] == 0x0 && data[3] == 0x1 &&
          (data[kNALUHeaderSize] & 0x7e) == 0x42);
}

scoped_refptr<DecoderBuffer> EncodedDataHelperH26x::GetNextBuffer() {
  if (next_pos_to_parse_ == 0) {
    if (!LookForSPS()) {
      next_pos_to_parse_ = 0;
      return nullptr;
    }
  }

  size_t start_pos = next_pos_to_parse_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_parse_.
  next_pos_to_parse_ = next_nalu_pos;
  return DecoderBuffer::CopyFrom(
      base::as_byte_span(data_).subspan(start_pos, next_nalu_pos - start_pos));
}

size_t EncodedDataHelperH26x::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + kNALUHeaderSize > data_.size()) {
    return pos;
  }
  if (!IsNALHeader(data_, pos)) {
    ADD_FAILURE();
    return std::numeric_limits<std::size_t>::max();
  }
  pos += kNALUHeaderSize;
  while (pos + kNALUHeaderSize <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + kNALUReducedHeaderSize >= data_.size()) {
    pos = data_.size();
  }
  return pos;
}

bool EncodedDataHelperH26x::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

bool EncodedDataHelperH26x::LookForSPS() {
  while (next_pos_to_parse_ + kNALUHeaderSize < data_.size()) {
    if (codec_ == VideoCodec::kH264 &&
        (data_[next_pos_to_parse_ + kNALUHeaderSize] & 0x1f) == 0x7) {
      return true;
    } else if (codec_ == VideoCodec::kHEVC &&
               (data_[next_pos_to_parse_ + kNALUHeaderSize] & 0x7e) == 0x42) {
      return true;
    }
    next_pos_to_parse_ = GetBytesForNextNALU(next_pos_to_parse_);
  }
  return false;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
EncodedDataHelperH265::EncodedDataHelperH265(base::span<const uint8_t> stream,
                                             VideoCodec codec)
    : EncodedDataHelper(std::move(stream), codec),
      h265_parser_(std::make_unique<H265Parser>()) {
  h265_parser_->SetStream(reinterpret_cast<uint8_t*>(data_.data()),
                          data_.size());
}

EncodedDataHelperH265::~EncodedDataHelperH265() = default;

scoped_refptr<DecoderBuffer> EncodedDataHelperH265::GetNextBuffer() {
  CHECK(h265_parser_);
  // This method is expected to send back DecoderBuffers with full frames, but
  // oftentimes NALUs are only slices and part of a frame. HEVC uses SPS/PPS/SEI
  // NALUs and "data" NALU's |first_slice_segment_in_pic_flag| flag to delimit
  // those full frames; these  mark what's loosely called "first-slice" (of a
  // frame) or a "(new) frame boundary":
  //
  // - VPS/SPS/PPS/SEI etc NALUs always mark a new frame boundary.
  // - A |first_slice_segment_in_pic_flag| of 1/true also marks a new frame
  //   boundary.
  // - A |first_slice_segment_in_pic_flag| of 0/false means that the current
  //   slice is part of a larger frame (and should be stored for later
  //   reassembly.
  //
  // Note how we can only tell that a full frame is seen when we parse the
  // _next_ NALU (or the stream ends). This complicates the code below greatly,
  // since it needs to accumulate NALUs for later reassembly.

  while (true) {
    H265NALU nalu;
    {
      const auto result = h265_parser_->AdvanceToNextNALU(&nalu);
      if (result == H265Parser::kEOStream) {
        // |h265_parser_| has consumed all the data that was passed to it. This
        // inherently signals a frame boundary.
        auto decoder_buffer = ReassembleNALUs(previous_nalus_);
        previous_nalus_.clear();
        return decoder_buffer;
      }
      if (result == H265Parser::kInvalidStream ||
          result == H265Parser::kUnsupportedStream) {
        LOG(ERROR) << __func__ << " Invalid or unsupported bitstream";
        return nullptr;
      }
      CHECK_EQ(result, H265Parser::kOk);
    }
    CHECK_LE(nalu.data,
             reinterpret_cast<uint8_t*>(data_.data()) + data_.size());
    CHECK_LE(nalu.data + nalu.size,
             reinterpret_cast<uint8_t*>(data_.data()) + data_.size());

    struct NALUMetadata nalu_metadata;
    nalu_metadata.start_pointer =
        reinterpret_cast<uint8_t*>(data_.data()) + next_pos_to_parse_;
    nalu_metadata.start_index = next_pos_to_parse_;
    nalu_metadata.header_size = nalu.data - nalu_metadata.start_pointer;
    nalu_metadata.size_with_header = nalu_metadata.header_size + nalu.size;
    VLOG(2) << "NALU (" << nalu.nal_unit_type << ") found " << nalu_metadata
            << " next_pos_to_parse_=" << next_pos_to_parse_;

    next_pos_to_parse_ += nalu_metadata.size_with_header;

    bool is_new_frame_boundary = false;
    switch (nalu.nal_unit_type) {
      case H265NALU::SPS_NUT: {
        int sps_id;
        const auto result = h265_parser_->ParseSPS(&sps_id);
        if (result != H265Parser::kOk) {
          LOG(ERROR) << __func__ << "Error parsing SPS";
          return nullptr;
        }
        is_new_frame_boundary = true;
        break;
      }
      case H265NALU::PPS_NUT: {
        int pps_id;
        const auto result = h265_parser_->ParsePPS(nalu, &pps_id);
        if (result != H265Parser::kOk) {
          LOG(ERROR) << __func__ << "Error parsing PPS";
          return nullptr;
        }
        is_new_frame_boundary = true;
        break;
      }
      case H265NALU::BLA_W_LP:
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      case H265NALU::CRA_NUT: {
        auto current_slice_header = std::make_unique<H265SliceHeader>();
        const auto result = h265_parser_->ParseSliceHeader(
            nalu, current_slice_header.get(), previous_slice_header_.get());
        if (result != H265Parser::kOk) {
          LOG(ERROR) << __func__ << "Error parsing slice header";
          return nullptr;
        }

        is_new_frame_boundary =
            current_slice_header->first_slice_segment_in_pic_flag;
        VLOG_IF(4, is_new_frame_boundary) << "|is_new_frame_boundary|";

        previous_slice_header_ = std::move(current_slice_header);
        break;
      }
      default:  // Not a special NALU. Assume it marks the start of a new frame.
        is_new_frame_boundary = true;
        break;
    }

    if (!is_new_frame_boundary) {
      VLOG(3) << "Storing current NALU " << nalu_metadata;
      previous_nalus_.push_back(std::move(nalu_metadata));
      continue;
    }

    const bool is_stand_alone_NALU = nalu.nal_unit_type >= H265NALU::VPS_NUT;
    if (previous_nalus_.empty() && is_stand_alone_NALU) {
      // Nothing stored from before, return the current NALU instantly (this is
      // the case for e.g. SPS/PPS/SEI).
      VLOG(3) << "Returning current NALU " << nalu_metadata;
      return DecoderBuffer::CopyFrom(base::as_byte_span(data_).subspan(
          nalu_metadata.start_index, nalu_metadata.size_with_header));
    }

    if (previous_nalus_.empty()) {
      VLOG(3) << "Storing current NALU " << nalu_metadata;
      previous_nalus_.push_back(std::move(nalu_metadata));
      continue;
    }

    // Accumulate what we have and send it; store |nalu_metadata| for later.
    auto decoder_buffer = ReassembleNALUs(previous_nalus_);
    previous_nalus_.clear();
    VLOG(3) << "Storing current NALU " << nalu_metadata;
    previous_nalus_.push_back(std::move(nalu_metadata));
    return decoder_buffer;
  }
}

bool EncodedDataHelperH265::ReachEndOfStream() const {
  return EncodedDataHelper::ReachEndOfStream() && previous_nalus_.empty();
}

void EncodedDataHelperH265::Rewind() {
  h265_parser_->Reset();
  h265_parser_->SetStream(reinterpret_cast<uint8_t*>(data_.data()),
                          data_.size());
  previous_nalus_.clear();
  EncodedDataHelper::Rewind();
}

scoped_refptr<DecoderBuffer> EncodedDataHelperH265::ReassembleNALUs(
    const std::vector<struct NALUMetadata>& nalus) {
  if (nalus.empty()) {
    return nullptr;
  }
  const size_t total_size = std::accumulate(
      nalus.begin(), nalus.end(), 0, [](size_t total, NALUMetadata metadata) {
        return total + metadata.size_with_header;
      });
  VLOG(4) << "Reassembling " << nalus.size() << " NALUs, " << total_size << "B";
  return DecoderBuffer::CopyFrom(base::as_byte_span(data_).subspan(
      nalus.begin()->start_index, total_size));
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

EncodedDataHelperIVF::EncodedDataHelperIVF(base::span<const uint8_t> stream,
                                           VideoCodec codec)
    : EncodedDataHelper(std::move(stream), codec) {}

scoped_refptr<DecoderBuffer> EncodedDataHelperIVF::GetNextBuffer() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  // Only IVF video files are supported. The first 4bytes of an IVF video file's
  // header should be "DKIF".
  if (next_pos_to_parse_ == 0) {
    if (data_.size() < kIvfFileHeaderSize) {
      LOG(ERROR) << "data is too small";
      return nullptr;
    }
    auto ivf_header = GetIvfFileHeader(base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(&data_[0]), kIvfFileHeaderSize));
    if (strncmp(ivf_header.signature, "DKIF", kNALUHeaderSize) != 0) {
      LOG(ERROR) << "Unexpected data encountered while parsing IVF header";
      return nullptr;
    }
    next_pos_to_parse_ = kIvfFileHeaderSize;  // Skip IVF header.
  }

  std::vector<IvfFrame> ivf_frames;
  auto frame_data = ReadNextIvfFrame();
  if (!frame_data) {
    LOG(ERROR) << "No IVF frame is available";
    return nullptr;
  }
  ivf_frames.push_back(*frame_data);

  if (codec_ == VideoCodec::kVP9 || codec_ == VideoCodec::kAV1) {
    // Group IVF data whose timestamps are the same in VP9 and AV1. Spatial
    // layers in a spatial-SVC stream may separately be stored in IVF data,
    // where the timestamps of the IVF frame headers are the same. However, it
    // is necessary for VD(A) to feed the spatial layers by a single
    // DecoderBuffer. So this grouping is required.
    while (!ReachEndOfStream()) {
      auto frame_header = GetNextIvfFrameHeader();
      if (!frame_header) {
        LOG(ERROR) << "No IVF frame header is available";
        return nullptr;
      }

      // Timestamp is different from the current one. The next IVF data must be
      // grouped in the next group.
      if (frame_header->timestamp != ivf_frames[0].header.timestamp)
        break;

      frame_data = ReadNextIvfFrame();
      if (!frame_data) {
        LOG(ERROR) << "No IVF frame is available";
        return nullptr;
      }
      ivf_frames.push_back(*frame_data);
    }
  }

  // Standard stream case.
  if (ivf_frames.size() == 1) {
    return DecoderBuffer::CopyFrom(
        // TODO(crbug.com/40284755): spanify `IvfFrame`.
        UNSAFE_TODO(base::span(ivf_frames[0].data.get(),
                               ivf_frames[0].header.frame_size)));
  }

  if (ivf_frames.size() > 3) {
    LOG(ERROR) << "Number of IVF frames with same timestamps exceeds maximum of"
               << "3: ivf_frames.size()=" << ivf_frames.size();
    return nullptr;
  }

  std::string data;
  std::vector<uint32_t> frame_sizes;
  frame_sizes.reserve(ivf_frames.size());
  for (const IvfFrame& ivf : ivf_frames) {
    data.append(reinterpret_cast<const char*>(ivf.data.get()),
                ivf.header.frame_size);
    frame_sizes.push_back(ivf.header.frame_size);
  }

  auto buffer = DecoderBuffer::CopyFrom(base::as_byte_span(data));
  buffer->WritableSideData().spatial_layers = frame_sizes;
  return buffer;
}

std::optional<IvfFrameHeader> EncodedDataHelperIVF::GetNextIvfFrameHeader()
    const {
  const size_t pos = next_pos_to_parse_;
  // Read VP8/9 frame size from IVF header.
  if (pos + kIvfFrameHeaderSize > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    return std::nullopt;
  }
  return GetIvfFrameHeader(base::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(&data_[pos]), kIvfFrameHeaderSize));
}

std::optional<IvfFrame> EncodedDataHelperIVF::ReadNextIvfFrame() {
  auto frame_header = GetNextIvfFrameHeader();
  if (!frame_header)
    return std::nullopt;

  // Skip IVF frame header.
  const size_t pos = next_pos_to_parse_ + kIvfFrameHeaderSize;

  // Make sure we are not reading out of bounds.
  if (pos + frame_header->frame_size > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    next_pos_to_parse_ = data_.size();
    return std::nullopt;
  }

  // Update next_pos_to_parse_.
  next_pos_to_parse_ = pos + frame_header->frame_size;

  return IvfFrame{*frame_header, reinterpret_cast<uint8_t*>(&data_[pos])};
}

struct AlignedDataHelper::VideoFrameData {
  VideoFrameData() = default;
  explicit VideoFrameData(base::ReadOnlySharedMemoryRegion shmem_region)
      : shmem_region(std::move(shmem_region)) {}
  explicit VideoFrameData(gfx::GpuMemoryBufferHandle gmb_handle)
      : gmb_handle(std::move(gmb_handle)) {}

  VideoFrameData(VideoFrameData&&) = default;
  VideoFrameData& operator=(VideoFrameData&&) = default;
  VideoFrameData(const VideoFrameData&) = delete;
  VideoFrameData& operator=(const VideoFrameData&) = delete;

  base::ReadOnlySharedMemoryRegion shmem_region;
  gfx::GpuMemoryBufferHandle gmb_handle;
};

AlignedDataHelper::AlignedDataHelper(const RawVideo* video,
                                     uint32_t num_read_frames,
                                     bool reverse,
                                     const gfx::Size& aligned_coded_size,
                                     const gfx::Size& natural_size,
                                     uint32_t frame_rate,
                                     VideoFrame::StorageType storage_type)
    : video_(video),
      num_frames_(video_->NumFrames()),
      num_read_frames_(num_read_frames),
      reverse_(reverse),
      create_frame_mode_(num_frames_ > RawVideo::kLimitedReadFrames
                             ? CreateFrameMode::kOnDemand
                             : CreateFrameMode::kAllAtOnce),
      storage_type_(storage_type),
      visible_rect_(video_->VisibleRect()),
      natural_size_(natural_size),
      time_stamp_interval_(base::Seconds(/*secs=*/0u)),
      elapsed_frame_time_(base::Seconds(/*secs=*/0u)) {
  // If the frame_rate is passed in, then use that timing information
  // to generate timestamps that increment according the frame_rate.
  // Otherwise timestamps will be generated when GetNextFrame() is called
  UpdateFrameRate(frame_rate);

  if (storage_type_ == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    layout_ = GetPlatformVideoFrameLayout(
        video_->PixelFormat(), aligned_coded_size,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  } else {
    layout_ = CreateVideoFrameLayout(video_->PixelFormat(), aligned_coded_size,
                                     kPlatformBufferAlignment);
  }
  LOG_ASSERT(layout_) << "Failed creating VideoFrameLayout";

  if (create_frame_mode_ == CreateFrameMode::kOnDemand) {
    return;
  }

  video_frame_data_.resize(num_frames_);
  for (size_t i = 0; i < num_frames_; i++) {
    video_frame_data_[i] = CreateVideoFrameData(
        storage_type_, video->GetFrame(i), video_->FrameLayout(), *layout_);
  }

  LOG_ASSERT(video_frame_data_.size() == num_frames_)
      << "Failed to initialize VideoFrames";
}

AlignedDataHelper::~AlignedDataHelper() {}

void AlignedDataHelper::Rewind() {
  frame_index_ = 0;
}

bool AlignedDataHelper::AtHeadOfStream() const {
  return frame_index_ == 0;
}

bool AlignedDataHelper::AtEndOfStream() const {
  return frame_index_ == num_read_frames_;
}

void AlignedDataHelper::UpdateFrameRate(uint32_t frame_rate) {
  if (frame_rate == 0) {
    time_stamp_interval_ = base::Seconds(/*secs=*/0u);
  } else {
    time_stamp_interval_ = base::Seconds(/*secs=*/1u) / frame_rate;
  }
}

scoped_refptr<VideoFrame> AlignedDataHelper::GetNextFrame() {
  LOG_ASSERT(!AtEndOfStream());
  base::TimeDelta frame_timestamp;

  if (time_stamp_interval_.is_zero())
    frame_timestamp = base::TimeTicks::Now().since_origin();
  else
    frame_timestamp = elapsed_frame_time_;

  elapsed_frame_time_ += time_stamp_interval_;

  const uint32_t read_frame_index =
      GetReadFrameIndex(frame_index_++, reverse_, num_frames_);
  if (create_frame_mode_ == CreateFrameMode::kOnDemand) {
    auto frame_data = video_->GetFrame(read_frame_index);
    VideoFrameData video_frame_data = CreateVideoFrameData(
        storage_type_, frame_data, video_->FrameLayout(), *layout_);
    return CreateVideoFrameFromVideoFrameData(video_frame_data,
                                              frame_timestamp);
  } else {
    return CreateVideoFrameFromVideoFrameData(
        video_frame_data_[read_frame_index], frame_timestamp);
  }
}

scoped_refptr<VideoFrame> AlignedDataHelper::CreateVideoFrameFromVideoFrameData(
    const VideoFrameData& video_frame_data,
    base::TimeDelta frame_timestamp) const {
  if (storage_type_ == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    const auto& gmb_handle = video_frame_data.gmb_handle;
    auto dup_handle = gmb_handle.Clone();
    if (dup_handle.is_null()) {
      LOG(ERROR) << "Failed duplicating GpuMemoryBufferHandle";
      return nullptr;
    }

    std::optional<gfx::BufferFormat> buffer_format =
        VideoPixelFormatToGfxBufferFormat(layout_->format());
    if (!buffer_format) {
      LOG(ERROR) << "Unexpected format: " << layout_->format();
      return nullptr;
    }

    // Create GpuMemoryBuffer from GpuMemoryBufferHandle.
    gpu::GpuMemoryBufferSupport support;
    auto gpu_memory_buffer = support.CreateGpuMemoryBufferImplFromHandle(
        std::move(dup_handle), layout_->coded_size(), *buffer_format,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        base::DoNothing());
    if (!gpu_memory_buffer) {
      LOG(ERROR) << "Failed to create GpuMemoryBuffer from "
                 << "GpuMemoryBufferHandle";
      return nullptr;
    }

    return media::VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect_, natural_size_, std::move(gpu_memory_buffer),
        frame_timestamp);
  } else {
    const auto& shmem_region = video_frame_data.shmem_region;
    auto dup_region = shmem_region.Duplicate();
    if (!dup_region.IsValid()) {
      LOG(ERROR) << "Failed duplicating shmem region";
      return nullptr;
    }
    base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
    uint8_t* buf = const_cast<uint8_t*>(mapping.GetMemoryAs<uint8_t>());
    uint8_t* data[3] = {};
    for (size_t i = 0; i < layout_->planes().size(); i++)
      data[i] = buf + layout_->planes()[i].offset;

    auto frame = media::VideoFrame::WrapExternalYuvDataWithLayout(
        *layout_, visible_rect_, natural_size_, data[0], data[1], data[2],
        frame_timestamp);
    DCHECK(frame);
    frame->BackWithOwnedSharedMemory(std::move(dup_region), std::move(mapping));
    return frame;
  }
}

// static
AlignedDataHelper::VideoFrameData AlignedDataHelper::CreateVideoFrameData(
    VideoFrame::StorageType storage_type,
    const RawVideo::FrameData& src_frame,
    const VideoFrameLayout& src_layout,
    const VideoFrameLayout& dst_layout) {
  LOG_ASSERT(gfx::Rect(dst_layout.coded_size())
                 .Contains(gfx::Rect(src_layout.coded_size())))
      << "The destination buffer resolution must not be smaller than the "
         "source buffer resolution";
  const VideoPixelFormat pixel_format = src_layout.format();
  const gfx::Size& resolution = src_layout.coded_size();
  if (storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    // First write into on-memory frame.
    auto memory_frame =
        VideoFrame::CreateFrame(pixel_format, resolution, gfx::Rect(resolution),
                                resolution, base::TimeDelta());
    LOG_ASSERT(!!memory_frame) << "Failed creating VideoFrame";
    for (size_t i = 0; i < src_layout.planes().size(); i++) {
      libyuv::CopyPlane(
          src_frame.plane_addrs[i], src_frame.strides[i],
          memory_frame->writable_data(i), memory_frame->stride(i),
          VideoFrame::RowBytes(i, pixel_format, resolution.width()),
          VideoFrame::Rows(i, pixel_format, resolution.height()));
    }
    // Create GpuMemoryBuffer VideoFrame from the on-memory VideoFrame.
    auto frame = CloneVideoFrame(
        memory_frame.get(), dst_layout, VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
    LOG_ASSERT(!!frame) << "Failed creating GpuMemoryBuffer VideoFrame";

    auto gmb_handle = CreateGpuMemoryBufferHandle(frame.get());
    LOG_ASSERT(!gmb_handle.is_null())
        << "Failed creating GpuMemoryBufferHandle";
    return VideoFrameData(std::move(gmb_handle));
#else
    NOTREACHED_IN_MIGRATION();
    return VideoFrameData();
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  } else {
    const size_t dst_video_frame_size =
        dst_layout.planes().back().offset + dst_layout.planes().back().size;
    auto mapped_region =
        base::ReadOnlySharedMemoryRegion::Create(dst_video_frame_size);
    LOG_ASSERT(mapped_region.IsValid()) << "Failed allocating a region";
    base::WritableSharedMemoryMapping& mapping = mapped_region.mapping;
    LOG_ASSERT(mapping.IsValid());
    uint8_t* buffer = mapping.GetMemoryAs<uint8_t>();
    for (size_t i = 0; i < src_layout.planes().size(); i++) {
      auto dst_plane_layout = dst_layout.planes()[i];
      uint8_t* dst_ptr = &buffer[dst_plane_layout.offset];
      libyuv::CopyPlane(
          src_frame.plane_addrs[i], src_frame.strides[i], dst_ptr,
          dst_plane_layout.stride,
          VideoFrame::RowBytes(i, pixel_format, resolution.width()),
          VideoFrame::Rows(i, pixel_format, resolution.height()));
    }
    return VideoFrameData(std::move(mapped_region.region));
  }
}

// static
RawDataHelper::RawDataHelper(const RawVideo* video, bool reverse)
    : video_(video), reverse_(reverse) {}

RawDataHelper::~RawDataHelper() = default;

scoped_refptr<const VideoFrame> RawDataHelper::GetFrame(size_t index) const {
  uint32_t read_frame_index =
      GetReadFrameIndex(index, reverse_, video_->NumFrames());
  uint8_t* frame_data[VideoFrame::kMaxPlanes] = {};
  const size_t num_planes = VideoFrame::NumPlanes(video_->PixelFormat());
  RawVideo::FrameData src_frame = video_->GetFrame(read_frame_index);
  for (size_t i = 0; i < num_planes; ++i) {
    // The data is never modified but WrapExternalYuvDataWithLayout() only
    // accepts non-const pointer.
    frame_data[i] = const_cast<uint8_t*>(src_frame.plane_addrs[i]);
  }

  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::WrapExternalYuvDataWithLayout(
          video_->FrameLayout(), video_->VisibleRect(),
          video_->VisibleRect().size(), frame_data[0], frame_data[1],
          frame_data[2], base::TimeTicks::Now().since_origin());
  video_frame->AddDestructionObserver(
      base::DoNothingWithBoundArgs(std::move(src_frame)));
  return video_frame;
}

}  // namespace test
}  // namespace media
