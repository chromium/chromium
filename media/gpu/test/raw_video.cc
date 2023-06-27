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
#include "media/base/media_serializers.h"
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

class VP9Decoder {
 public:
  static std::unique_ptr<VP9Decoder> Create(
      const base::FilePath& vp9_webm_data_file_path,
      const VideoFrameLayout& layout,
      size_t num_read_frames);

  std::vector<uint8_t> DecodeNextFrame() {
    // If this is the first decode, then starts the thread.
    if (!decoder_thread_.IsRunning()) {
      LOG_IF(FATAL, !decoder_thread_.Start())
          << "Failed to start decoder thread";
      DecoderStatus result;
      base::WaitableEvent event;
      // base::Unretained(this) is safe because this is blocking call.
      decoder_thread_.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&VP9Decoder::InitializeTask,
                                    base::Unretained(this), &result, &event));
      event.Wait();
      LOG_ASSERT(result.is_ok())
          << "Failed to initialize VpxVideoDecoder: " << MediaSerialize(result)
          << "with config=" << config_.AsHumanReadableString();
    }

    std::vector<uint8_t> decoded_frame_buffer;
    base::WaitableEvent done;
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VP9Decoder::DecodeNextFrameTask, base::Unretained(this),
                       &decoded_frame_buffer, &done));
    done.Wait();
    CHECK(!decoded_frame_buffer.empty());
    return decoded_frame_buffer;
  }

  ~VP9Decoder() {
    if (decoder_thread_.IsRunning()) {
      decoder_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                                std::move(vpx_decoder_));
    }
  }

 private:
  VP9Decoder(std::unique_ptr<base::MemoryMappedFile> vp9_data_mmap_file,
             const std::vector<base::span<const uint8_t>>& vp9_data_chunks,
             const VideoDecoderConfig& config,
             const VideoFrameLayout& layout)
      : vp9_data_mmap_file_(std::move(vp9_data_mmap_file)),
        config_(config),
        layout_(layout),
        decoder_thread_("VP9DecoderThread"),
        vp9_data_chunks_(vp9_data_chunks) {
    DETACH_FROM_SEQUENCE(decoder_sequence_);
  }

  void OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_);
    last_decoded_frame_ = std::move(frame);
  }

  void InitializeTask(DecoderStatus* result, base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_);
    vpx_decoder_ = std::make_unique<VpxVideoDecoder>(
        OffloadableVideoDecoder::OffloadState::kOffloaded);
    vpx_decoder_->Initialize(
        config_,
        /*low_delay*/ false,
        /*CdmContext*/ nullptr,
        base::BindOnce([](DecoderStatus* save_to,
                          DecoderStatus save_from) { *save_to = save_from; },
                       result),
        // base::Unretained(this) is safe because |vpx_decoder_| is owned by
        // this.
        base::BindRepeating(&VP9Decoder::OnFrameDecoded,
                            base::Unretained(this)),
        /*waiting_cb=*/base::NullCallback());
    event->Signal();
  }

  void DecodeNextFrameTask(std::vector<uint8_t>* decoded_frame_buffer,
                           base::WaitableEvent* done) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_);
    CHECK_LT(next_frame_index_, vp9_data_chunks_.size());
    base::span<const uint8_t> chunk = vp9_data_chunks_[next_frame_index_];
    DecoderStatus decode_status{DecoderStatus::Codes::kOk};
    vpx_decoder_->Decode(
        DecoderBuffer::CopyFrom(chunk.data(), chunk.size()),
        base::BindOnce([](DecoderStatus* out_status,
                          DecoderStatus status) { *out_status = status; },
                       &decode_status));
    LOG_ASSERT(decode_status.is_ok())
        << "Failed to decode the " << next_frame_index_
        << "-th vp9 chunk: " << MediaSerialize(decode_status);
    LOG_ASSERT(!!last_decoded_frame_) << "|last_decoded_frame_| is not filled";
    *decoded_frame_buffer = CreateBufferFromFrame(*last_decoded_frame_);
    last_decoded_frame_.reset();
    next_frame_index_++;
    done->Signal();
  }

  std::vector<uint8_t> CreateBufferFromFrame(
      const VideoFrame& i420_frame) const {
    LOG_ASSERT(i420_frame.format() == VideoPixelFormat::PIXEL_FORMAT_I420);
    std::vector<uint8_t> buffer;
    buffer.resize(layout_.planes()[2].offset + layout_.planes()[2].size);
    // Copy the resolution area.
    uint8_t* dst_plane = buffer.data();
    for (size_t plane = 0; plane < 3; ++plane) {
      const int stride = i420_frame.stride(plane);
      const int rows = VideoFrame::Rows(plane, i420_frame.format(),
                                        layout_.coded_size().height());
      const int row_bytes = VideoFrame::RowBytes(plane, i420_frame.format(),
                                                 layout_.coded_size().width());
      // VideoFrame::PlaneSize() cannot be used because it computes the
      // plane size with resolutions aligned by two while our test code
      // works with a succinct buffer size.
      const uint8_t* src = i420_frame.data(plane);
      libyuv::CopyPlane(src, stride, dst_plane, row_bytes, row_bytes, rows);
      dst_plane += (rows * row_bytes);
    }
    return buffer;
  }

  const std::unique_ptr<base::MemoryMappedFile> vp9_data_mmap_file_;
  const VideoDecoderConfig config_;
  const VideoFrameLayout layout_;

  base::Thread decoder_thread_;
  const std::vector<base::span<const uint8_t>> vp9_data_chunks_
      GUARDED_BY_CONTEXT(decoder_sequence_);
  std::unique_ptr<VpxVideoDecoder> vpx_decoder_
      GUARDED_BY_CONTEXT(decoder_sequence_);
  size_t next_frame_index_ GUARDED_BY_CONTEXT(decoder_sequence_){0};
  scoped_refptr<VideoFrame> last_decoded_frame_
      GUARDED_BY_CONTEXT(decoder_sequence_);
  SEQUENCE_CHECKER(decoder_sequence_);
};

// static
std::unique_ptr<VP9Decoder> VP9Decoder::Create(
    const base::FilePath& vp9_webm_data_file_path,
    const VideoFrameLayout& layout,
    size_t num_read_frames) {
  base::MemoryMappedFile vp9_webm_data_mmap_file;
  if (!vp9_webm_data_mmap_file.Initialize(vp9_webm_data_file_path,
                                          base::MemoryMappedFile::READ_ONLY)) {
    LOG(ERROR) << "Failed to read file: " << vp9_webm_data_file_path;
    return nullptr;
  }
  base::span<const uint8_t> vp9_webm_data(vp9_webm_data_mmap_file.data(),
                                          vp9_webm_data_mmap_file.length());

  InitializeMediaLibrary();

  // Initialize ffmpeg with the compressed video data.
  InMemoryUrlProtocol protocol(vp9_webm_data.data(), vp9_webm_data.size(),
                               /*streaming=*/false);
  FFmpegGlue glue(&protocol);
  LOG_ASSERT(glue.OpenContext()) << "Failed to open AVFormatContext";
  // Find the first VP9 stream in the file.
  absl::optional<size_t> vp9_stream_index;
  VideoDecoderConfig config;
  for (size_t i = 0; i < glue.format_context()->nb_streams; ++i) {
    AVStream* stream = glue.format_context()->streams[i];
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;
    const AVCodecID codec_id = codec_parameters->codec_id;
    if (codec_type == AVMEDIA_TYPE_VIDEO && codec_id == AV_CODEC_ID_VP9 &&
        AVStreamToVideoDecoderConfig(stream, &config) &&
        config.IsValidConfig()) {
      vp9_stream_index = i;
      break;
    }
  }
  if (!vp9_stream_index) {
    return nullptr;
  }

  auto vp9_data_mmap_file = CreateMemoryMappedFile(vp9_webm_data.size());
  uint8_t* const vp9_data = vp9_data_mmap_file->data();
  size_t vp9_data_size = 0;
  AVPacket packet = {};
  size_t num_packets = 0;
  std::vector<base::span<const uint8_t>> vp9_data_chunks(num_read_frames);
  while (av_read_frame(glue.format_context(), &packet) >= 0 &&
         num_packets < num_read_frames) {
    if (base::checked_cast<size_t>(packet.stream_index) ==
        (*vp9_stream_index)) {
      LOG_ASSERT(vp9_data_size + packet.size <= vp9_data_mmap_file->length())
          << "The vp9 data size must be less than webm file size";
      std::memcpy(vp9_data + vp9_data_size, packet.data, packet.size);
      vp9_data_chunks[num_packets] = base::span<const uint8_t>(
          vp9_data + vp9_data_size, base::checked_cast<size_t>(packet.size));
      vp9_data_size += packet.size;
      num_packets++;
    }
    av_packet_unref(&packet);
  }
  return base::WrapUnique<VP9Decoder>(new VP9Decoder(
      std::move(vp9_data_mmap_file), vp9_data_chunks, config, layout));
}

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
    auto vp9_decoder = VP9Decoder::Create(
        data_file_path, *metadata.frame_layout, metadata.num_frames);
    memory_mapped_file =
        CreateMemoryMappedFile(video_frame_size * metadata.num_frames);
    for (size_t i = 0; i < metadata.num_frames; ++i) {
      auto buffer = vp9_decoder->DecodeNextFrame();
      memcpy(memory_mapped_file->data() + i * video_frame_size, buffer.data(),
             buffer.size());
    }
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
  LOG_ASSERT(memory_mapped_file_) << "CreateNV12Video() is supported only if "
                                  << "|memory_mapped_file_| is valid";
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
