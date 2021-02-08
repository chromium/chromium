// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_test_helpers.h"

#include <limits>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/format_utils.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame_layout.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/h264_parser.h"
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
}  // namespace

IvfFileHeader GetIvfFileHeader(const base::span<const uint8_t>& data) {
  LOG_ASSERT(data.size_bytes() == 32u);
  IvfFileHeader file_header;
  memcpy(&file_header, data.data(), sizeof(IvfFileHeader));
  file_header.ByteSwap();
  return file_header;
}

IvfFrameHeader GetIvfFrameHeader(const base::span<const uint8_t>& data) {
  LOG_ASSERT(data.size_bytes() == 12u);
  IvfFrameHeader frame_header{};
  memcpy(&frame_header.frame_size, data.data(), 4);
  memcpy(&frame_header.timestamp, &data[4], 8);
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
  char ivf_header[kIvfFileHeaderSize] = {};
  // Bytes 0-3 of an IVF file header always contain the signature 'DKIF'.
  strcpy(&ivf_header[0], "DKIF");
  constexpr uint16_t kVersion = 0;
  auto write16 = [&ivf_header](int i, uint16_t v) {
    memcpy(&ivf_header[i], &v, sizeof(v));
  };
  auto write32 = [&ivf_header](int i, uint32_t v) {
    memcpy(&ivf_header[i], &v, sizeof(v));
  };

  write16(4, kVersion);
  write16(6, kIvfFileHeaderSize);
  switch (codec) {
    case kCodecVP8:
      strcpy(&ivf_header[8], "VP80");
      break;
    case kCodecVP9:
      strcpy(&ivf_header[8], "VP90");
      break;
    default:
      LOG(ERROR) << "Unknown codec: " << GetCodecName(codec);
      return false;
  }

  write16(12, resolution.width());
  write16(14, resolution.height());
  write32(16, frame_rate);
  write32(20, 1);
  write32(24, num_frames);
  // Reserved.
  write32(28, 0);
  return output_file_.WriteAtCurrentPos(ivf_header, kIvfFileHeaderSize) ==
         static_cast<int>(kIvfFileHeaderSize);
}

bool IvfWriter::WriteFrame(uint32_t data_size,
                           uint64_t timestamp,
                           const uint8_t* data) {
  char ivf_frame_header[kIvfFrameHeaderSize] = {};
  memcpy(&ivf_frame_header[0], &data_size, sizeof(data_size));
  memcpy(&ivf_frame_header[4], &timestamp, sizeof(timestamp));
  bool success =
      output_file_.WriteAtCurrentPos(ivf_frame_header, kIvfFrameHeaderSize) ==
          static_cast<int>(kIvfFrameHeaderSize) &&
      output_file_.WriteAtCurrentPos(reinterpret_cast<const char*>(data),
                                     data_size) == static_cast<int>(data_size);
  return success;
}

EncodedDataHelper::EncodedDataHelper(const std::vector<uint8_t>& stream,
                                     VideoCodecProfile profile)
    : data_(std::string(reinterpret_cast<const char*>(stream.data()),
                        stream.size())),
      profile_(profile) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

bool EncodedDataHelper::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextBuffer() {
  switch (VideoCodecProfileToVideoCodec(profile_)) {
    case kCodecH264:
      return GetNextFragment();
    case kCodecVP8:
    case kCodecVP9:
    case kCodecAV1:
      return GetNextFrame();
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextFragment() {
  if (next_pos_to_decode_ == 0) {
    size_t skipped_fragments_count = 0;
    if (!LookForSPS(&skipped_fragments_count)) {
      next_pos_to_decode_ = 0;
      return nullptr;
    }
    num_skipped_fragments_ += skipped_fragments_count;
  }

  size_t start_pos = next_pos_to_decode_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = next_nalu_pos;
  return DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&data_[start_pos]),
      next_nalu_pos - start_pos);
}

size_t EncodedDataHelper::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + 4 > data_.size())
    return pos;
  if (!IsNALHeader(data_, pos)) {
    ADD_FAILURE();
    return std::numeric_limits<std::size_t>::max();
  }
  pos += 4;
  while (pos + 4 <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + 3 >= data_.size())
    pos = data_.size();
  return pos;
}

bool EncodedDataHelper::LookForSPS(size_t* skipped_fragments_count) {
  *skipped_fragments_count = 0;
  while (next_pos_to_decode_ + 4 < data_.size()) {
    if ((data_[next_pos_to_decode_ + 4] & 0x1f) == 0x7) {
      return true;
    }
    *skipped_fragments_count += 1;
    next_pos_to_decode_ = GetBytesForNextNALU(next_pos_to_decode_);
  }
  return false;
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextFrame() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  // Only IVF video files are supported. The first 4bytes of an IVF video file's
  // header should be "DKIF".
  if (next_pos_to_decode_ == 0) {
    if (data_.size() < kIvfFileHeaderSize) {
      LOG(ERROR) << "data is too small";
      return nullptr;
    }
    auto ivf_header = GetIvfFileHeader(base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(&data_[0]), kIvfFileHeaderSize));
    if (strncmp(ivf_header.signature, "DKIF", 4) != 0) {
      LOG(ERROR) << "Unexpected data encountered while parsing IVF header";
      return nullptr;
    }
    next_pos_to_decode_ = kIvfFileHeaderSize;  // Skip IVF header.
  }

  // Group IVF data whose timestamps are the same. Spatial layers in a
  // spatial-SVC stream may separately be stored in IVF data, where the
  // timestamps of the IVF frame headers are the same. However, it is necessary
  // for VD(A) to feed the spatial layers by a single DecoderBuffer. So this
  // grouping is required.
  std::vector<IvfFrame> ivf_frames;
  while (!ReachEndOfStream()) {
    auto frame_header = GetNextIvfFrameHeader();
    if (!frame_header)
      return nullptr;

    // Timestamp is different from the current one. The next IVF data must be
    // grouped in the next group.
    if (!ivf_frames.empty() &&
        frame_header->timestamp != ivf_frames[0].header.timestamp) {
      break;
    }

    auto frame_data = ReadNextIvfFrame();
    if (!frame_data)
      return nullptr;

    ivf_frames.push_back(*frame_data);
  }

  if (ivf_frames.empty()) {
    LOG(ERROR) << "No IVF frame is available";
    return nullptr;
  }

  // Standard stream case.
  if (ivf_frames.size() == 1) {
    return DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8_t*>(ivf_frames[0].data),
        ivf_frames[0].header.frame_size);
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
    data.append(reinterpret_cast<char*>(ivf.data), ivf.header.frame_size);
    frame_sizes.push_back(ivf.header.frame_size);
  }

  // Copy frame_sizes information to DecoderBuffer's side data. Since side_data
  // is uint8_t*, we need to copy as uint8_t from uint32_t. The copied data is
  // recognized as uint32_t in VD(A).
  const uint8_t* side_data =
      reinterpret_cast<const uint8_t*>(frame_sizes.data());
  size_t side_data_size =
      frame_sizes.size() * sizeof(uint32_t) / sizeof(uint8_t);

  return DecoderBuffer::CopyFrom(reinterpret_cast<const uint8_t*>(data.data()),
                                 data.size(), side_data, side_data_size);
}

base::Optional<IvfFrameHeader> EncodedDataHelper::GetNextIvfFrameHeader()
    const {
  const size_t pos = next_pos_to_decode_;
  // Read VP8/9 frame size from IVF header.
  if (pos + kIvfFrameHeaderSize > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    return base::nullopt;
  }
  return GetIvfFrameHeader(base::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(&data_[pos]), kIvfFrameHeaderSize));
}

base::Optional<IvfFrame> EncodedDataHelper::ReadNextIvfFrame() {
  auto frame_header = GetNextIvfFrameHeader();
  if (!frame_header)
    return base::nullopt;

  // Skip IVF frame header.
  const size_t pos = next_pos_to_decode_ + kIvfFrameHeaderSize;

  // Make sure we are not reading out of bounds.
  if (pos + frame_header->frame_size > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    next_pos_to_decode_ = data_.size();
    return base::nullopt;
  }

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = pos + frame_header->frame_size;

  return IvfFrame{*frame_header, reinterpret_cast<uint8_t*>(&data_[pos])};
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    Vp8Parser parser;
    Vp8FrameHeader frame_header;
    if (!parser.ParseFrame(data, size, &frame_header)) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }
    // Stream configuration is present in a keyframe in vp8.
    return frame_header.IsKeyframe();
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    Vp9Parser parser(false);
    parser.SetStream(data, size, nullptr);
    Vp9FrameHeader frame_header;
    std::unique_ptr<DecryptConfig> null_config;
    gfx::Size allocated_size;
    Vp9Parser::Result result =
        parser.ParseNextFrame(&frame_header, &allocated_size, &null_config);
    if (result != Vp9Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }
    // Stream configuration is present in a keyframe in vp9.
    return frame_header.IsKeyframe();
  } else if (profile >= AV1PROFILE_MIN && profile <= AV1PROFILE_MAX) {
    // TODO(hiroh): Implement this.
    return false;
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << GetProfileName(profile);
  return false;
}

struct AlignedDataHelper::VideoFrameData {
  VideoFrameData() = default;
  VideoFrameData(mojo::ScopedSharedBufferHandle mojo_handle)
      : mojo_handle(std::move(mojo_handle)) {}
  VideoFrameData(gfx::GpuMemoryBufferHandle gmb_handle)
      : gmb_handle(std::move(gmb_handle)) {}

  VideoFrameData(VideoFrameData&&) = default;
  VideoFrameData& operator=(VideoFrameData&&) = default;
  VideoFrameData(const VideoFrameData&) = delete;
  VideoFrameData& operator=(const VideoFrameData&) = delete;

  mojo::ScopedSharedBufferHandle mojo_handle;
  gfx::GpuMemoryBufferHandle gmb_handle;
};

AlignedDataHelper::AlignedDataHelper(
    const std::vector<uint8_t>& stream,
    uint32_t num_frames,
    VideoPixelFormat pixel_format,
    const gfx::Size& src_coded_size,
    const gfx::Size& dst_coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint32_t frame_rate,
    VideoFrame::StorageType storage_type,
    gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory)
    : num_frames_(num_frames),
      storage_type_(storage_type),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      time_stamp_interval_(base::TimeDelta::FromSeconds(/*secs=*/0u)),
      elapsed_frame_time_(base::TimeDelta::FromSeconds(/*secs=*/0u)) {
  // If the frame_rate is passed in, then use that timing information
  // to generate timestamps that increment according the frame_rate.
  // Otherwise timestamps will be generated when GetNextFrame() is called
  UpdateFrameRate(frame_rate);

  if (storage_type_ == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    LOG_ASSERT(gpu_memory_buffer_factory_ != nullptr);
    InitializeGpuMemoryBufferFrames(stream, pixel_format, src_coded_size,
                                    dst_coded_size);
  } else {
    LOG_ASSERT(storage_type == VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
    InitializeAlignedMemoryFrames(stream, pixel_format, src_coded_size,
                                  dst_coded_size);
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
  return frame_index_ == num_frames_;
}

void AlignedDataHelper::UpdateFrameRate(uint32_t frame_rate) {
  if (frame_rate == 0) {
    time_stamp_interval_ = base::TimeDelta::FromSeconds(/*secs=*/0u);
  } else {
    time_stamp_interval_ =
        base::TimeDelta::FromSeconds(/*secs=*/1u) / frame_rate;
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

  if (storage_type_ == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    const auto& gmb_handle = video_frame_data_[frame_index_++].gmb_handle;
    auto dup_handle = gmb_handle.Clone();
    if (dup_handle.is_null()) {
      LOG(ERROR) << "Failed duplicating GpuMemoryBufferHandle";
      return nullptr;
    }

    base::Optional<gfx::BufferFormat> buffer_format =
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

    gpu::MailboxHolder dummy_mailbox[media::VideoFrame::kMaxPlanes];
    return media::VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect_, natural_size_, std::move(gpu_memory_buffer),
        dummy_mailbox, base::DoNothing() /* mailbox_holder_release_cb_ */,
        frame_timestamp);
  } else {
    const auto& mojo_handle = video_frame_data_[frame_index_++].mojo_handle;
    auto dup_handle =
        mojo_handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);
    if (!dup_handle.is_valid()) {
      LOG(ERROR) << "Failed duplicating mojo handle";
      return nullptr;
    }

    std::vector<uint32_t> offsets(layout_->planes().size());
    std::vector<int32_t> strides(layout_->planes().size());
    for (size_t i = 0; i < layout_->planes().size(); i++) {
      offsets[i] = layout_->planes()[i].offset;
      strides[i] = layout_->planes()[i].stride;
    }
    const size_t video_frame_size =
        layout_->planes().back().offset + layout_->planes().back().size;
    return MojoSharedBufferVideoFrame::Create(
        layout_->format(), layout_->coded_size(), visible_rect_, natural_size_,
        std::move(dup_handle), video_frame_size, offsets, strides,
        frame_timestamp);
  }
}

void AlignedDataHelper::InitializeAlignedMemoryFrames(
    const std::vector<uint8_t>& stream,
    const VideoPixelFormat pixel_format,
    const gfx::Size& src_coded_size,
    const gfx::Size& dst_coded_size) {
  ASSERT_NE(pixel_format, PIXEL_FORMAT_UNKNOWN);

  // Calculate padding in bytes to be added after each plane required to keep
  // starting addresses of all planes at a byte boundary required by the
  // platform. This padding will be added after each plane when copying to the
  // temporary file.
  // At the same time we also need to take into account coded_size requested by
  // the VEA; each row of |src_strides| bytes in the original file needs to be
  // copied into a row of |strides_| bytes in the aligned file.
  size_t video_frame_size;
  layout_ = GetAlignedVideoFrameLayout(pixel_format, dst_coded_size,
                                       kPlatformBufferAlignment, nullptr,
                                       &video_frame_size);
  LOG_ASSERT(video_frame_size > 0UL);

  std::vector<size_t> src_plane_rows;
  size_t src_video_frame_size = 0;
  auto src_layout = GetAlignedVideoFrameLayout(
      pixel_format, src_coded_size, 1u /* alignment */, &src_plane_rows,
      &src_video_frame_size);
  LOG_ASSERT(stream.size() % src_video_frame_size == 0U)
      << "Stream byte size is not a product of calculated frame byte size";

  LOG_ASSERT(video_frame_size > 0UL);
  video_frame_data_.resize(num_frames_);
  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  const uint8_t* src_frame_ptr = &stream[0];
  for (size_t i = 0; i < num_frames_; i++) {
    auto handle = mojo::SharedBufferHandle::Create(video_frame_size);
    ASSERT_TRUE(handle.is_valid()) << "Failed allocating a handle";
    auto mapping = handle->Map(video_frame_size);
    ASSERT_TRUE(!!mapping);
    uint8_t* buffer = reinterpret_cast<uint8_t*>(mapping.get());
    for (size_t i = 0; i < num_planes; i++) {
      auto src_plane_layout = src_layout.planes()[i];
      auto dst_plane_layout = layout_->planes()[i];
      const uint8_t* src_ptr = src_frame_ptr + src_plane_layout.offset;
      uint8_t* dst_ptr = &buffer[dst_plane_layout.offset];
      libyuv::CopyPlane(src_ptr, src_plane_layout.stride, dst_ptr,
                        dst_plane_layout.stride, src_plane_layout.stride,
                        src_plane_rows[i]);
    }
    src_frame_ptr += src_video_frame_size;
    video_frame_data_[i] = VideoFrameData(std::move(handle));
  }
}

void AlignedDataHelper::InitializeGpuMemoryBufferFrames(
    const std::vector<uint8_t>& stream,
    const VideoPixelFormat pixel_format,
    const gfx::Size& src_coded_size,
    const gfx::Size& dst_coded_size) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  layout_ = GetPlatformVideoFrameLayout(
      gpu_memory_buffer_factory_, pixel_format, dst_coded_size,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
  ASSERT_TRUE(layout_) << "Failed getting platform VideoFrameLayout";

  std::vector<size_t> src_plane_rows;
  size_t src_video_frame_size = 0;
  auto src_layout = GetAlignedVideoFrameLayout(
      pixel_format, src_coded_size, 1u /* alignment */, &src_plane_rows,
      &src_video_frame_size);
  LOG_ASSERT(stream.size() % src_video_frame_size == 0U)
      << "Stream byte size is not a product of calculated frame byte size";

  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  const uint8_t* src_frame_ptr = &stream[0];
  for (size_t i = 0; i < num_frames_; i++) {
    auto memory_frame =
        VideoFrame::CreateFrame(pixel_format, dst_coded_size, visible_rect_,
                                natural_size_, base::TimeDelta());
    LOG_ASSERT(!!memory_frame) << "Failed creating VideoFrame";
    for (size_t i = 0; i < num_planes; i++) {
      libyuv::CopyPlane(src_frame_ptr + src_layout.planes()[i].offset,
                        src_layout.planes()[i].stride, memory_frame->data(i),
                        memory_frame->stride(i), src_layout.planes()[i].stride,
                        src_plane_rows[i]);
    }
    src_frame_ptr += src_video_frame_size;
    auto frame =
        CloneVideoFrame(gpu_memory_buffer_factory_, memory_frame.get(),
                        *layout_, VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
    LOG_ASSERT(!!frame) << "Failed creating GpuMemoryBuffer VideoFrame";
    auto gmb_handle = CreateGpuMemoryBufferHandle(frame.get());
    LOG_ASSERT(!gmb_handle.is_null())
        << "Failed creating GpuMemoryBufferHandle";
    video_frame_data_.push_back(VideoFrameData(std::move(gmb_handle)));
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
}

// static
VideoFrameLayout AlignedDataHelper::GetAlignedVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& dimension,
    const uint32_t alignment,
    std::vector<size_t>* plane_rows,
    size_t* video_frame_size) {
  auto layout =
      CreateVideoFrameLayout(pixel_format, dimension, alignment, plane_rows);
  LOG_ASSERT(layout) << "Failed creating VideoFrameLayout";
  if (video_frame_size) {
    const auto& plane = layout->planes().back();
    *video_frame_size = plane.offset + plane.size;
  }
  return *layout;
}

// static
std::unique_ptr<RawDataHelper> RawDataHelper::Create(Video* video) {
  size_t frame_size = 0;
  VideoPixelFormat pixel_format = video->PixelFormat();
  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  size_t strides[VideoFrame::kMaxPlanes] = {};
  size_t plane_sizes[VideoFrame::kMaxPlanes] = {};
  const gfx::Size& resolution = video->Resolution();
  // Calculate size of frames and their planes.
  for (size_t i = 0; i < num_planes; ++i) {
    const size_t bytes_per_line =
        VideoFrame::RowBytes(i, pixel_format, resolution.width());
    const size_t plane_size =
        bytes_per_line * VideoFrame::Rows(i, pixel_format, resolution.height());
    strides[i] = bytes_per_line;
    plane_sizes[i] = plane_size;
    frame_size += plane_size;
  }

  // Verify whether calculated frame size is valid.
  const size_t data_size = video->Data().size();
  if (frame_size == 0 || data_size % frame_size != 0 ||
      data_size / frame_size != video->NumFrames()) {
    LOG(ERROR) << "Invalid frame_size=" << frame_size
               << ", file size=" << data_size;
    return nullptr;
  }

  std::vector<ColorPlaneLayout> planes(num_planes);
  size_t offset = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride =
        VideoFrame::RowBytes(i, pixel_format, resolution.width());
    planes[i].offset = offset;
    planes[i].size = plane_sizes[i];
    offset += plane_sizes[i];
  }
  auto layout = VideoFrameLayout::CreateWithPlanes(pixel_format, resolution,
                                                   std::move(planes));
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  return base::WrapUnique(new RawDataHelper(video, frame_size, *layout));
}

RawDataHelper::RawDataHelper(Video* video,
                             size_t frame_size,
                             const VideoFrameLayout& layout)
    : video_(video), frame_size_(frame_size), layout_(layout) {}

RawDataHelper::~RawDataHelper() = default;

scoped_refptr<const VideoFrame> RawDataHelper::GetFrame(size_t index) {
  if (index >= video_->NumFrames()) {
    LOG(ERROR) << "index is too big. index=" << index
               << ", num_frames=" << video_->NumFrames();
    return nullptr;
  }

  size_t offset = frame_size_ * index;
  uint8_t* frame_data[VideoFrame::kMaxPlanes] = {};
  const size_t num_planes = VideoFrame::NumPlanes(video_->PixelFormat());
  for (size_t i = 0; i < num_planes; ++i) {
    frame_data[i] = reinterpret_cast<uint8_t*>(video_->Data().data()) + offset;
    offset += layout_->planes()[i].size;
  }

  // TODO(crbug.com/1045825): Investigate use of MOJO_SHARED_BUFFER, similar to
  // changes made in crrev.com/c/2050895.
  scoped_refptr<const VideoFrame> video_frame =
      VideoFrame::WrapExternalYuvDataWithLayout(
          *layout_, video_->VisibleRect(), video_->VisibleRect().size(),
          frame_data[0], frame_data[1], frame_data[2],
          base::TimeTicks::Now().since_origin());
  return video_frame;
}

}  // namespace test
}  // namespace media
