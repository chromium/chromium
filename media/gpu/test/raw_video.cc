// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/raw_video.h"

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/base/media.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/offloading_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ffmpeg/libavformat/avformat.h"
#include "third_party/ffmpeg/libavutil/avutil.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media::test {

namespace {

// Suffix appended to the video file path to get the metadata file path, if no
// explicit metadata file path was specified.
constexpr const base::FilePath::CharType* kMetadataSuffix =
    FILE_PATH_LITERAL(".json");

std::unique_ptr<base::MemoryMappedFile> CreateMemoryMappedFile(size_t size) {
  base::FilePath tmp_file_path;
  if (!base::CreateTemporaryFile(&tmp_file_path)) {
    LOG(ERROR) << "Failed to create a temporary file";
    return nullptr;
  }
  auto mmapped_file = std::make_unique<base::MemoryMappedFile>();
  bool success = mmapped_file->Initialize(
      base::File(tmp_file_path,
                 base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                     base::File::FLAG_WRITE | base::File::FLAG_APPEND),
      base::MemoryMappedFile::Region{0, size},
      base::MemoryMappedFile::READ_WRITE_EXTEND);
  base::DeleteFile(tmp_file_path);
  return success ? std::move(mmapped_file) : nullptr;
}

void DecodeVP9Task(base::span<const uint8_t> data,
                   const gfx::Size& resolution,
                   const size_t num_frames,
                   const size_t video_frame_size,
                   uint8_t* dst_buffer,
                   bool* success,
                   base::WaitableEvent* done) {
  *success = false;
  InitializeMediaLibrary();

  // Initialize ffmpeg with the compressed video data.
  InMemoryUrlProtocol protocol(&data[0], data.size(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  // Find the first VP9 stream in the file.
  int stream_index = -1;
  VideoDecoderConfig config;
  for (size_t i = 0; i < glue.format_context()->nb_streams; ++i) {
    AVStream* stream = glue.format_context()->streams[i];
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;
    const AVCodecID codec_id = codec_parameters->codec_id;
    if (codec_type == AVMEDIA_TYPE_VIDEO && codec_id == AV_CODEC_ID_VP9) {
      *success = AVStreamToVideoDecoderConfig(stream, &config) &&
                 config.IsValidConfig();
      stream_index = i;
      break;
    }
  }
  if (!*success) {
    done->Signal();
    return;
  }

  auto on_frame_decoded = [](const gfx::Size& resolution,
                             const size_t video_frame_size,
                             uint8_t* const dst_buffer,
                             size_t* decode_frame_count,
                             scoped_refptr<VideoFrame> frame) {
    ASSERT_EQ(frame->format(), VideoPixelFormat::PIXEL_FORMAT_I420);
    size_t num_planes = VideoFrame::NumPlanes(frame->format());
    uint8_t* dst_plane = dst_buffer + video_frame_size * (*decode_frame_count);
    // Copy the resolution area.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      const int stride = frame->stride(plane);
      const int rows =
          VideoFrame::Rows(plane, frame->format(), resolution.height());
      const int row_bytes =
          VideoFrame::RowBytes(plane, frame->format(), resolution.width());
      // VideoFrame::PlaneSize() cannot be used because it computes the
      // plane size with resolutions aligned by two while our test code
      // works with a succinct buffer size.
      const uint8_t* src = frame->data(plane);
      libyuv::CopyPlane(src, stride, dst_plane, row_bytes, row_bytes, rows);
      dst_plane += (rows * row_bytes);
    }
    *decode_frame_count += 1;
  };

  size_t decode_frame_count = 0;
  // Setup the VP9 decoder.
  DecoderStatus init_result;
  VpxVideoDecoder decoder(
      media::OffloadableVideoDecoder::OffloadState::kOffloaded);
  media::VideoDecoder::InitCB init_cb =
      base::BindOnce([](DecoderStatus* save_to,
                        DecoderStatus save_from) { *save_to = save_from; },
                     &init_result);
  decoder.Initialize(
      config, false, nullptr, std::move(init_cb),
      base::BindRepeating(on_frame_decoded, resolution, video_frame_size,
                          dst_buffer, &decode_frame_count),
      base::NullCallback());
  if (!init_result.is_ok()) {
    done->Signal();
    return;
  }

  // Start decoding and wait until all frames are ready.
  AVPacket packet = {};
  size_t num_decoded_frames = 0;
  while (av_read_frame(glue.format_context(), &packet) >= 0 &&
         num_decoded_frames < num_frames) {
    if (packet.stream_index == stream_index) {
      media::VideoDecoder::DecodeCB decode_cb = base::BindOnce(
          [](bool* success, DecoderStatus status) {
            *success = (status.is_ok());
          },
          success);
      decoder.Decode(DecoderBuffer::CopyFrom(packet.data, packet.size),
                     std::move(decode_cb));
      if (!*success) {
        break;
      }
      num_decoded_frames++;
    }
    av_packet_unref(&packet);
  }

  done->Signal();
}

// Decode the vp9 data and returns the raw I420 data. Returns empty vector on
// fatal.
std::unique_ptr<base::MemoryMappedFile> DecodeAndLoadVP9Data(
    const base::FilePath& data_file_path,
    const gfx::Size& resolution,
    size_t video_frame_size,
    size_t num_read_frames) {
  base::MemoryMappedFile compressed_data_mmap_file;
  if (!compressed_data_mmap_file.Initialize(
          data_file_path, base::MemoryMappedFile::READ_ONLY)) {
    LOG(ERROR) << "Failed to read file: " << data_file_path;
    return nullptr;
  }
  base::span<const uint8_t> compressed_data(compressed_data_mmap_file.data(),
                                            compressed_data_mmap_file.length());

  auto decompressed_data_mmap_file =
      CreateMemoryMappedFile(video_frame_size * num_read_frames);
  if (!decompressed_data_mmap_file) {
    LOG(ERROR) << "Failed to create memory mapped file for decompressed data";
    return nullptr;
  }
  // The VpxVideoDecoder requires running on a SequencedTaskRunner, so we
  // can't decode the video on the main test thread.
  base::Thread decode_thread("DecodeThread");
  if (!decode_thread.Start()) {
    LOG(ERROR) << "Failed to start decode thread";
    return nullptr;
  }

  bool success = false;
  base::WaitableEvent done;
  decode_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DecodeVP9Task, compressed_data, resolution,
                     num_read_frames, video_frame_size,
                     decompressed_data_mmap_file->data(), &success, &done));
  done.Wait();
  decode_thread.Stop();
  if (!success) {
    return nullptr;
  }
  return decompressed_data_mmap_file;
}

std::unique_ptr<base::MemoryMappedFile> LoadRawData(
    const base::FilePath& data_file_path,
    size_t video_frame_size,
    size_t num_read_frames) {
  auto memory_mapped_file = std::make_unique<base::MemoryMappedFile>();
  if (!memory_mapped_file->Initialize(
          base::File(data_file_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ),
          base::MemoryMappedFile::Region{0, video_frame_size * num_read_frames},
          base::MemoryMappedFile::READ_ONLY)) {
    LOG(ERROR) << "Failed to read the file: " << data_file_path;
    return nullptr;
  }
  CHECK_EQ(memory_mapped_file->length(), video_frame_size * num_read_frames);
  return memory_mapped_file;
}
}  // namespace

RawVideo::RawVideo(std::unique_ptr<base::MemoryMappedFile> memory_mapped_file,
                   const Metadata& metadata,
                   size_t video_frame_size)
    : memory_mapped_file_(std::move(memory_mapped_file)),
      metadata_(metadata),
      video_frame_size_(video_frame_size) {}

RawVideo::~RawVideo() = default;

RawVideo::Metadata::Metadata() = default;
RawVideo::Metadata::~Metadata() = default;
RawVideo::Metadata::Metadata(const Metadata&) = default;
RawVideo::Metadata& RawVideo::Metadata::operator=(const Metadata&) = default;

RawVideo::FrameData::FrameData(const std::vector<const uint8_t*>& plane_addrs,
                               const std::vector<size_t>& strides)
    : plane_addrs(plane_addrs), strides(strides) {}

RawVideo::FrameData::FrameData(FrameData&& frame_data)
    : plane_addrs(frame_data.plane_addrs), strides(frame_data.strides) {}

RawVideo::FrameData::~FrameData() = default;

// Load the metadata from |json_file_path|. The read metadata is filled into
// |metadata| and compressed_data is set to true if the metadata denotes the
// |video| is vp9 video.
// static
bool RawVideo::LoadMetadata(const base::FilePath& json_file_path,
                            Metadata& metadata,
                            bool& is_vp9_data) {
  std::string json_data;
  if (!base::ReadFileToString(json_file_path, &json_data)) {
    LOG(ERROR) << "Failed to read video metadata file: " << json_file_path;
    return false;
  }

  auto metadata_result =
      base::JSONReader::ReadAndReturnValueWithError(json_data);
  if (!metadata_result.has_value()) {
    LOG(ERROR) << "Failed to parse video metadata: " << json_file_path << ": "
               << metadata_result.error().message;
    return false;
  }
  base::Value::Dict& metadata_dict = metadata_result->GetDict();

  // The json must have either "profile" or "pixel_format".
  // If it has "profile", then the data file is vp9 webm.
  // If it has "pixel_format", then the data file is I420.
  const std::string* profile = metadata_dict.FindString("profile");
  const std::string* pixel_format = metadata_dict.FindString("pixel_format");
  if (!!profile == !!pixel_format) {
    LOG(ERROR) << "Metadata file must have either profile or pixel_format";
    return false;
  }
  if (profile && *profile != "VP9PROFILE_PROFILE0") {
    LOG(ERROR) << "The compressed video data file must be VP9 profile 0";
    return false;
  }
  if (pixel_format && *pixel_format != "I420") {
    LOG(ERROR) << "The raw video data file must be I420";
    return false;
  }
  is_vp9_data = !!profile;

  absl::optional<int> frame_rate = metadata_dict.FindInt("frame_rate");
  if (!frame_rate.has_value()) {
    LOG(ERROR) << "Key \"frame_rate\" is not found in " << json_file_path;
    return false;
  }
  metadata.frame_rate = base::checked_cast<uint32_t>(*frame_rate);

  absl::optional<int> num_frames = metadata_dict.FindInt("num_frames");
  if (!num_frames.has_value()) {
    LOG(ERROR) << "Key \"num_frames\" is not found in " << json_file_path;
    return false;
  }
  metadata.num_frames = base::checked_cast<size_t>(*num_frames);

  absl::optional<int> width = metadata_dict.FindInt("width");
  if (!width.has_value()) {
    LOG(ERROR) << "Key \"width\" is not found in " << json_file_path;
    return false;
  }
  absl::optional<int> height = metadata_dict.FindInt("height");
  if (!height) {
    LOG(ERROR) << "Key \"height\" is not found in " << json_file_path;
    return false;
  }

  const gfx::Size resolution(static_cast<uint32_t>(*width),
                             static_cast<uint32_t>(*height));
  metadata.frame_layout =
      CreateVideoFrameLayout(PIXEL_FORMAT_I420, resolution, 1u /* alignment */);

  // The default visible rectangle is (0, 0, |resolution_|). Expand() needs to
  // be called to change the visible rectangle.
  metadata.visible_rect = gfx::Rect(resolution);

  return true;
}

// static
std::unique_ptr<RawVideo> RawVideo::Create(
    const base::FilePath& file_path,
    const base::FilePath& metadata_file_path,
    bool read_all_frames) {
  CHECK(!file_path.empty());
  const base::FilePath data_file_path = ResolveFilePath(file_path);
  if (data_file_path.empty()) {
    LOG(ERROR) << "Video file not found: " << file_path;
    return nullptr;
  }
  const base::FilePath json_file_path = ResolveFilePath(
      metadata_file_path.empty() ? file_path.AddExtension(kMetadataSuffix)
                                 : metadata_file_path);
  if (json_file_path.empty()) {
    LOG(ERROR) << "Metadata file not found: " << file_path;
    return nullptr;
  }

  bool is_vp9_data;
  RawVideo::Metadata metadata;
  if (!LoadMetadata(json_file_path, metadata, is_vp9_data)) {
    LOG(ERROR) << "Invalid metadata file: " << json_file_path;
    return nullptr;
  }

  std::vector<size_t> plane_offsets;
  size_t video_frame_size = 0;
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_I420;
  const gfx::Size& resolution = metadata.frame_layout->coded_size();
  for (size_t i = 0; i < VideoFrame::NumPlanes(kPixelFormat); ++i) {
    video_frame_size +=
        VideoFrame::RowBytes(i, kPixelFormat, resolution.width()) *
        VideoFrame::Rows(i, kPixelFormat, resolution.height());
  }
  LOG_ASSERT(video_frame_size ==
             metadata.frame_layout->planes().back().offset +
                 metadata.frame_layout->planes().back().size)
      << " video frame size computed by media::VideoFrame is different from"
      << " one computed by media::VideoFrameLayout";

  if (!read_all_frames && metadata.num_frames > kLimitedReadFrames) {
    DLOG(WARNING) << "Limit video length to " << kLimitedReadFrames
                  << " frames";
    metadata.num_frames = kLimitedReadFrames;
  }

  std::unique_ptr<base::MemoryMappedFile> memory_mapped_file;
  if (is_vp9_data) {
    // If the given data is compressed video (i.e. vp9 webm), then we decode.
    memory_mapped_file = DecodeAndLoadVP9Data(
        data_file_path, resolution, video_frame_size, metadata.num_frames);
  } else {
    memory_mapped_file =
        LoadRawData(data_file_path, video_frame_size, metadata.num_frames);
  }
  if (!memory_mapped_file) {
    return nullptr;
  }

  return base::WrapUnique(
      new RawVideo(std::move(memory_mapped_file), metadata, video_frame_size));
}

std::unique_ptr<RawVideo> RawVideo::CreateNV12Video() const {
  LOG_ASSERT(FrameLayout().format() == PIXEL_FORMAT_I420)
      << "The pixel format of source video is not I420";
  auto nv12_layout = CreateVideoFrameLayout(PIXEL_FORMAT_NV12, Resolution(),
                                            1u /* alignment*/);
  LOG_ASSERT(nv12_layout) << "Failed creating VideoFrameLayout";
  auto new_memory_mapped_file =
      CreateMemoryMappedFile(NumFrames() * video_frame_size_);
  LOG_ASSERT(new_memory_mapped_file) << "Failed creating memory mapped file";
  for (size_t i = 0; i < NumFrames(); ++i) {
    const FrameData i420_frame = GetFrame(i);
    uint8_t* const nv12_frame =
        new_memory_mapped_file->data() + i * video_frame_size_;
    int ret =
        libyuv::I420ToNV12(i420_frame.plane_addrs[0], i420_frame.strides[0],
                           i420_frame.plane_addrs[1], i420_frame.strides[1],
                           i420_frame.plane_addrs[2], i420_frame.strides[2],
                           nv12_frame + nv12_layout->planes()[0].offset,
                           nv12_layout->planes()[0].stride,
                           nv12_frame + nv12_layout->planes()[1].offset,
                           nv12_layout->planes()[1].stride,
                           Resolution().width(), Resolution().height());
    LOG_ASSERT(ret == 0) << "Failed converting from I420 to NV12";
  }

  Metadata new_metadata = metadata_;
  new_metadata.frame_layout = nv12_layout;
  return base::WrapUnique(new RawVideo(std::move(new_memory_mapped_file),
                                       new_metadata, video_frame_size_));
}

std::unique_ptr<RawVideo> RawVideo::CreateExpandedVideo(
    const gfx::Size& resolution,
    const gfx::Rect& visible_rect) const {
  LOG_ASSERT(PixelFormat() == VideoPixelFormat::PIXEL_FORMAT_NV12)
      << "The pixel format of source video is not NV12";
  LOG_ASSERT(visible_rect.size() == Resolution())
      << "The resolution is different from the copied-into area of visible "
      << "rectangle";
  LOG_ASSERT(gfx::Rect(resolution).Contains(visible_rect))
      << "The resolution doesn't contain visible rectangle";
  LOG_ASSERT(visible_rect.x() % 2 == 0 && visible_rect.y() % 2 == 0)
      << "An odd origin point is not supported";
  const absl::optional<VideoFrameLayout> dst_layout =
      CreateVideoFrameLayout(PIXEL_FORMAT_NV12, resolution, 1u /* alignment*/);
  LOG_ASSERT(dst_layout) << "Failed creating VideoFrameLayout";
  const auto& dst_planes = dst_layout->planes();

  auto compute_dst_visible_data_offset = [&dst_layout,
                                          &visible_rect](size_t plane) {
    const size_t stride = dst_layout->planes()[plane].stride;
    const size_t bytes_per_pixel =
        VideoFrame::BytesPerElement(dst_layout->format(), plane);
    gfx::Point origin = visible_rect.origin();
    LOG_ASSERT(dst_layout->format() == VideoPixelFormat::PIXEL_FORMAT_NV12)
        << "The pixel format of destination video is not NV12";
    if (plane == 1) {
      origin.SetPoint(origin.x() / 2, origin.y() / 2);
    }
    return stride * origin.y() + bytes_per_pixel * origin.x();
  };
  const size_t dst_y_visible_offset = compute_dst_visible_data_offset(0);
  const size_t dst_uv_visible_offset = compute_dst_visible_data_offset(1);
  const size_t new_video_frame_size =
      dst_planes.back().offset + dst_planes.back().size;

  auto new_memory_mapped_file =
      CreateMemoryMappedFile(new_video_frame_size * NumFrames());
  CHECK(new_memory_mapped_file);
  for (size_t i = 0; i < NumFrames(); i++) {
    uint8_t* const dst_frame =
        new_memory_mapped_file->data() + (i * new_video_frame_size);
    uint8_t* const dst_y_plane_visible_data =
        dst_frame + dst_planes[0].offset + dst_y_visible_offset;
    uint8_t* const dst_uv_plane_visible_data =
        dst_frame + dst_planes[1].offset + dst_uv_visible_offset;
    FrameData src_frame = GetFrame(i);
    libyuv::NV12Copy(src_frame.plane_addrs[0], src_frame.strides[0],
                     src_frame.plane_addrs[1], src_frame.strides[1],
                     dst_y_plane_visible_data, dst_planes[1].stride,
                     dst_uv_plane_visible_data, dst_planes[1].stride,
                     visible_rect.width(), visible_rect.height());
  }

  Metadata new_metadata = metadata_;
  new_metadata.frame_layout = *dst_layout;
  new_metadata.visible_rect = visible_rect;
  return base::WrapUnique(new RawVideo(std::move(new_memory_mapped_file),
                                       new_metadata, new_video_frame_size));
}

RawVideo::FrameData RawVideo::GetFrame(size_t frame_index) const {
  CHECK_LT(frame_index, NumFrames());
  const uint8_t* frame_addr =
      memory_mapped_file_->data() + video_frame_size_ * frame_index;
  const auto& plane_layouts = FrameLayout().planes();
  const size_t num_planes = plane_layouts.size();
  std::vector<const uint8_t*> plane_addrs(num_planes);
  std::vector<size_t> strides(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    plane_addrs[i] = frame_addr + plane_layouts[i].offset;
    strides[i] = plane_layouts[i].stride;
  }

  return RawVideo::FrameData(plane_addrs, strides);
}

// static
base::FilePath RawVideo::test_data_path_;

// static
void RawVideo::SetTestDataPath(const base::FilePath& test_data_path) {
  test_data_path_ = test_data_path;
}

// static
base::FilePath RawVideo::ResolveFilePath(const base::FilePath& file_path) {
  base::FilePath resolved_path = file_path;

  // Try to resolve the path into an absolute path. If the path doesn't exist,
  // it might be relative to the test data dir.
  if (!resolved_path.IsAbsolute()) {
    resolved_path = base::MakeAbsoluteFilePath(
        PathExists(resolved_path) ? resolved_path
                                  : test_data_path_.Append(resolved_path));
  }

  return base::PathExists(resolved_path) ? resolved_path : base::FilePath();
}
}  // namespace media::test
