// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video.h"

#include <memory>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace media {
namespace test {

// Suffix appended to the video file path to get the metadata file path, if no
// explicit metadata file path was specified.
constexpr const base::FilePath::CharType* kMetadataSuffix =
    FILE_PATH_LITERAL(".json");

base::FilePath Video::test_data_path_ = base::FilePath();

Video::Video(const base::FilePath& file_path,
             const base::FilePath& metadata_file_path)
    : file_path_(file_path), metadata_file_path_(metadata_file_path) {}

Video::~Video() = default;

std::unique_ptr<Video> Video::Expand(const gfx::Size& resolution,
                                     const gfx::Rect& visible_rect) const {
  LOG_ASSERT(IsLoaded()) << "The source video is not loaded";
  LOG_ASSERT(pixel_format_ == VideoPixelFormat::PIXEL_FORMAT_NV12)
      << "The pixel format of source video is not NV12";
  LOG_ASSERT(visible_rect.size() == resolution_)
      << "The resolution is different from the copied-into area of visible "
      << "rectangle";
  LOG_ASSERT(gfx::Rect(resolution).Contains(visible_rect))
      << "The resolution doesn't contain visible rectangle";
  LOG_ASSERT(visible_rect.x() % 2 == 0 && visible_rect.y() % 2 == 0)
      << "An odd origin point is not supported";
  auto new_video = std::make_unique<Video>(file_path_, metadata_file_path_);
  new_video->frame_checksums_ = frame_checksums_;
  new_video->profile_ = profile_;
  new_video->codec_ = codec_;
  new_video->frame_rate_ = frame_rate_;
  new_video->num_frames_ = num_frames_;
  new_video->num_fragments_ = num_fragments_;
  new_video->resolution_ = resolution;
  new_video->visible_rect_ = visible_rect;
  new_video->pixel_format_ = pixel_format_;

  const auto src_layout =
      CreateVideoFrameLayout(PIXEL_FORMAT_NV12, resolution_, 1u /* alignment*/);
  const auto dst_layout =
      CreateVideoFrameLayout(PIXEL_FORMAT_NV12, resolution, 1u /* alignment*/);
  const size_t src_frame_size =
      src_layout->planes().back().offset + src_layout->planes().back().size;
  const size_t dst_frame_size =
      dst_layout->planes().back().offset + dst_layout->planes().back().size;
  LOG_ASSERT(src_frame_size * num_frames_ == data_.size())
      << "Unexpected data size";
  std::vector<uint8_t> new_data(dst_frame_size * num_frames_);
  auto compute_dst_visible_data_offset = [&dst_layout,
                                          &visible_rect](size_t plane) {
    const size_t stride = dst_layout->planes()[plane].stride;
    const size_t bytes_per_pixel =
        VideoFrame::BytesPerElement(dst_layout->format(), plane);
    gfx::Point origin = visible_rect.origin();
    LOG_ASSERT(dst_layout->format() == VideoPixelFormat::PIXEL_FORMAT_NV12)
        << "The pixel format of destination video is not NV12";
    if (plane == 1)
      origin.SetPoint(origin.x() / 2, origin.y() / 2);
    return stride * origin.y() + bytes_per_pixel * origin.x();
  };
  const size_t dst_y_visible_offset = compute_dst_visible_data_offset(0);
  const size_t dst_uv_visible_offset = compute_dst_visible_data_offset(1);
  for (size_t i = 0; i < num_frames_; i++) {
    const uint8_t* src_plane = data_.data() + (i * src_frame_size);
    uint8_t* const dst_plane = new_data.data() + (i * dst_frame_size);
    uint8_t* const dst_y_plane_visible_data =
        dst_plane + dst_layout->planes()[0].offset + dst_y_visible_offset;
    uint8_t* const dst_uv_plane_visible_data =
        dst_plane + dst_layout->planes()[1].offset + dst_uv_visible_offset;
    // Copy the source buffer to the visible area of the destination buffer.
    // libyuv::NV12Scale copies the source to the destination as-is when their
    // resolutions are the same.
    libyuv::NV12Scale(src_plane + src_layout->planes()[0].offset,
                      src_layout->planes()[0].stride,
                      src_plane + src_layout->planes()[1].offset,
                      src_layout->planes()[1].stride, resolution_.width(),
                      resolution_.height(), dst_y_plane_visible_data,
                      dst_layout->planes()[0].stride, dst_uv_plane_visible_data,
                      dst_layout->planes()[1].stride, visible_rect_.width(),
                      visible_rect_.height(),
                      libyuv::FilterMode::kFilterBilinear);
  }
  new_video->data_ = std::move(new_data);
  return new_video;
}

std::unique_ptr<Video> Video::ConvertToNV12() const {
  LOG_ASSERT(IsLoaded()) << "The source video is not loaded";
  LOG_ASSERT(pixel_format_ == VideoPixelFormat::PIXEL_FORMAT_I420)
      << "The pixel format of source video is not I420";
  auto new_video = std::make_unique<Video>(file_path_, metadata_file_path_);
  new_video->frame_checksums_ = frame_checksums_;
  new_video->profile_ = profile_;
  new_video->codec_ = codec_;
  new_video->bit_depth_ = bit_depth_;
  new_video->frame_rate_ = frame_rate_;
  new_video->num_frames_ = num_frames_;
  new_video->num_fragments_ = num_fragments_;
  new_video->resolution_ = resolution_;
  new_video->visible_rect_ = visible_rect_;
  new_video->pixel_format_ = PIXEL_FORMAT_NV12;

  // Convert I420 To NV12.
  const auto i420_layout = CreateVideoFrameLayout(
      PIXEL_FORMAT_I420, resolution_, 1u /* alignment */);
  const auto nv12_layout =
      CreateVideoFrameLayout(PIXEL_FORMAT_NV12, resolution_, 1u /* alignment*/);
  LOG_ASSERT(i420_layout && nv12_layout) << "Failed creating VideoFrameLayout";
  const size_t i420_frame_size =
      i420_layout->planes().back().offset + i420_layout->planes().back().size;
  const size_t nv12_frame_size =
      nv12_layout->planes().back().offset + nv12_layout->planes().back().size;
  LOG_ASSERT(i420_frame_size * num_frames_ == data_.size())
      << "Unexpected data size";
  std::vector<uint8_t> new_data(nv12_frame_size * num_frames_);
  for (size_t i = 0; i < num_frames_; i++) {
    const uint8_t* src_plane = data_.data() + (i * i420_frame_size);
    uint8_t* dst_plane = new_data.data() + (i * nv12_frame_size);
    libyuv::I420ToNV12(src_plane + i420_layout->planes()[0].offset,
                       i420_layout->planes()[0].stride,
                       src_plane + i420_layout->planes()[1].offset,
                       i420_layout->planes()[1].stride,
                       src_plane + i420_layout->planes()[2].offset,
                       i420_layout->planes()[2].stride,
                       dst_plane + nv12_layout->planes()[0].offset,
                       nv12_layout->planes()[0].stride,
                       dst_plane + nv12_layout->planes()[1].offset,
                       nv12_layout->planes()[1].stride, resolution_.width(),
                       resolution_.height());
  }
  new_video->data_ = std::move(new_data);
  return new_video;
}

bool Video::Load(const size_t max_frames) {
  // TODO(dstaessens@) Investigate reusing existing infrastructure such as
  //                   DecoderBuffer.
  DCHECK(!file_path_.empty());
  DCHECK(data_.empty());

  absl::optional<base::FilePath> resolved_path = ResolveFilePath(file_path_);
  if (!resolved_path) {
    LOG(ERROR) << "Video file not found: " << file_path_;
    return false;
  }
  file_path_ = resolved_path.value();
  VLOGF(2) << "File path: " << file_path_;

  int64_t file_size;
  if (!base::GetFileSize(file_path_, &file_size) || (file_size < 0)) {
    LOG(ERROR) << "Failed to read file size: " << file_path_;
    return false;
  }

  std::vector<uint8_t> data(file_size);
  if (base::ReadFile(file_path_, reinterpret_cast<char*>(data.data()),
                     base::checked_cast<int>(file_size)) != file_size) {
    LOG(ERROR) << "Failed to read file: " << file_path_;
    return false;
  }

  data_ = std::move(data);

  if (!LoadMetadata()) {
    LOG(ERROR) << "Failed to load metadata";
    return false;
  }

  if (num_frames_ <= max_frames) {
    return true;
  }

  DLOG(WARNING) << "Limiting video length to " << max_frames << " frames";
  // Limits the video length to the specified number of frames.
  if (pixel_format_ == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    // Compressed data. The unused frames are dropped in Decode().
    num_frames_ = max_frames;
    return true;
  }

  // Limits the video length here if the file is YUV.
  size_t video_frame_size = 0;
  for (size_t i = 0; i < VideoFrame::NumPlanes(pixel_format_); ++i) {
    video_frame_size +=
        VideoFrame::RowBytes(i, pixel_format_, resolution_.width()) *
        VideoFrame::Rows(i, pixel_format_, resolution_.height());
  }
  if (video_frame_size * num_frames_ != static_cast<size_t>(file_size)) {
    LOG(ERROR) << "Invalid file. file_size=" << file_size
               << ", expected file size=" << video_frame_size * num_frames_
               << ", video_frame_size=" << video_frame_size
               << ", num_frames_=" << num_frames_;
  }
  num_frames_ = max_frames;
  data_.resize(video_frame_size * max_frames);
  return true;
}

bool Video::Decode() {
  if (codec_ != VideoCodec::kVP9) {
    LOG(ERROR) << "Decoding is currently only supported for VP9 videos";
    return false;
  }
  if (bit_depth_ != 8u) {
    LOG(ERROR) << "Decoding is currently only supported for 8bpp videos";
    return false;
  }

  // The VpxVideoDecoder requires running on a SequencedTaskRunner, so we can't
  // decode the video on the main test thread.
  base::Thread decode_thread("DecodeThread");
  if (!decode_thread.Start()) {
    LOG(ERROR) << "Failed to start decode thread";
    return false;
  }

  std::vector<uint8_t> decompressed_data;
  bool success = false;
  base::WaitableEvent done;
  decode_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Video::DecodeTask, std::move(data_), resolution_,
                     num_frames_, &decompressed_data, &success, &done));
  done.Wait();
  decode_thread.Stop();

  if (!success)
    return false;

  // Set the video's pixel format and clear the profile and codec as the encoded
  // data will be replaced with the decompressed video stream.
  pixel_format_ = VideoPixelFormat::PIXEL_FORMAT_I420;
  profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  codec_ = VideoCodec::kUnknown;
  data_ = std::move(decompressed_data);
  return true;
}

bool Video::IsLoaded() const {
  return data_.size() > 0;
}

const base::FilePath& Video::FilePath() const {
  return file_path_;
}

const std::vector<uint8_t>& Video::Data() const {
  return data_;
}

std::vector<uint8_t>& Video::Data() {
  return data_;
}

VideoCodec Video::Codec() const {
  return codec_;
}

VideoCodecProfile Video::Profile() const {
  return profile_;
}

uint8_t Video::BitDepth() const {
  return bit_depth_;
}

VideoPixelFormat Video::PixelFormat() const {
  return pixel_format_;
}

uint32_t Video::FrameRate() const {
  return frame_rate_;
}

uint32_t Video::NumFrames() const {
  return num_frames_;
}

uint32_t Video::NumFragments() const {
  return num_fragments_;
}

gfx::Size Video::Resolution() const {
  return resolution_;
}

gfx::Rect Video::VisibleRect() const {
  return visible_rect_;
}

base::TimeDelta Video::GetDuration() const {
  return base::Seconds(static_cast<double>(num_frames_) /
                       static_cast<double>(frame_rate_));
}

const std::vector<std::string>& Video::FrameChecksums() const {
  return frame_checksums_;
}

// static
void Video::SetTestDataPath(const base::FilePath& test_data_path) {
  test_data_path_ = test_data_path;
}

bool Video::LoadMetadata() {
  if (IsMetadataLoaded()) {
    LOG(ERROR) << "Video metadata is already loaded";
    return false;
  }

  // If no custom metadata file path was specified, use <video_path>.json.
  if (metadata_file_path_.empty())
    metadata_file_path_ = file_path_.AddExtension(kMetadataSuffix);

  absl::optional<base::FilePath> resolved_path =
      ResolveFilePath(metadata_file_path_);
  if (!resolved_path) {
    LOG(ERROR) << "Video metadata file not found: " << metadata_file_path_;
    return false;
  }
  metadata_file_path_ = resolved_path.value();

  std::string json_data;
  if (!base::ReadFileToString(metadata_file_path_, &json_data)) {
    LOG(ERROR) << "Failed to read video metadata file: " << metadata_file_path_;
    return false;
  }

  auto metadata_result =
      base::JSONReader::ReadAndReturnValueWithError(json_data);
  if (!metadata_result.has_value()) {
    LOG(ERROR) << "Failed to parse video metadata: " << metadata_file_path_
               << ": " << metadata_result.error().message;
    return false;
  }
  base::Value::Dict& metadata = metadata_result->GetDict();

  // Find the video's profile, only required for encoded video streams.
  profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  const std::string* profile = metadata.FindString("profile");
  if (profile) {
    auto converted_profile = ConvertStringtoProfile(*profile);
    if (!converted_profile) {
      LOG(ERROR) << *profile << " is not supported";
      return false;
    }
    profile_ = converted_profile.value();

    auto converted_codec = ConvertProfileToCodec(profile_);
    if (!converted_codec) {
      LOG(ERROR) << *profile << " is not supported";
      return false;
    }
    codec_ = converted_codec.value();
  }

  // Find the video's bit depth. This is optional and only required for encoded
  // video streams.
  absl::optional<int> bit_depth = metadata.FindInt("bit_depth");
  if (bit_depth.has_value()) {
    bit_depth_ = base::checked_cast<uint8_t>(*bit_depth);
  } else {
    if (profile_ == VP9PROFILE_PROFILE2) {
      LOG(ERROR) << "Bit depth is unspecified for VP9 profile 2";
      return false;
    }
    constexpr uint8_t kDefaultBitDepth = 8u;
    bit_depth_ = kDefaultBitDepth;
  }

  // Find the video's pixel format, only required for raw video streams.
  pixel_format_ = VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
  const std::string* pixel_format = metadata.FindString("pixel_format");
  if (pixel_format) {
    auto converted_pixel_format = ConvertStringtoPixelFormat(*pixel_format);
    if (!converted_pixel_format) {
      LOG(ERROR) << *pixel_format << " is not supported";
      return false;
    }
    pixel_format_ = converted_pixel_format.value();
  }

  // We need to either know the video's profile (encoded video stream) or pixel
  // format (raw video stream).
  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN &&
      pixel_format_ == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    LOG(ERROR) << "No video profile or pixel format found";
    return false;
  }
  if (profile_ != VIDEO_CODEC_PROFILE_UNKNOWN &&
      pixel_format_ != VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    LOG(ERROR) << "Both video profile and pixel format are specified";
    return false;
  }

  absl::optional<int> frame_rate = metadata.FindInt("frame_rate");
  if (!frame_rate.has_value()) {
    LOG(ERROR) << "Key \"frame_rate\" is not found in " << metadata_file_path_;
    return false;
  }
  frame_rate_ = static_cast<uint32_t>(*frame_rate);

  absl::optional<int> num_frames = metadata.FindInt("num_frames");
  if (!num_frames.has_value()) {
    LOG(ERROR) << "Key \"num_frames\" is not found in " << metadata_file_path_;
    return false;
  }
  num_frames_ = static_cast<uint32_t>(*num_frames);

  // Find the number of fragments, only required for H.264/HEVC video streams.
  num_fragments_ = num_frames_;
  if ((profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) ||
      (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX)) {
    absl::optional<int> num_fragments = metadata.FindInt("num_fragments");
    if (!num_fragments.has_value()) {
      LOG(ERROR) << "Key \"num_fragments\" is required for H.264/HEVC video "
                    "streams but could not be found in "
                 << metadata_file_path_;
      return false;
    }
    num_fragments_ = static_cast<uint32_t>(*num_fragments);
  }

  absl::optional<int> width = metadata.FindInt("width");
  if (!width.has_value()) {
    LOG(ERROR) << "Key \"width\" is not found in " << metadata_file_path_;
    return false;
  }
  absl::optional<int> height = metadata.FindInt("height");
  if (!height) {
    LOG(ERROR) << "Key \"height\" is not found in " << metadata_file_path_;
    return false;
  }
  resolution_ =
      gfx::Size(static_cast<uint32_t>(*width), static_cast<uint32_t>(*height));
  // The default visible rectangle is (0, 0, |resolution_|). Expand() needs to
  // be called to change the visible rectangle.
  visible_rect_ = gfx::Rect(resolution_);

  // Find optional frame checksums. These are only required when using the frame
  // validator.
  const base::Value::List* md5_checksums = metadata.FindList("md5_checksums");
  if (md5_checksums) {
    for (const base::Value& checksum : *md5_checksums) {
      frame_checksums_.push_back(checksum.GetString());
    }
  }

  return true;
}

bool Video::IsMetadataLoaded() const {
  return profile_ != VIDEO_CODEC_PROFILE_UNKNOWN || num_frames_ != 0;
}

absl::optional<base::FilePath> Video::ResolveFilePath(
    const base::FilePath& file_path) {
  base::FilePath resolved_path = file_path;

  // Try to resolve the path into an absolute path. If the path doesn't exist,
  // it might be relative to the test data dir.
  if (!resolved_path.IsAbsolute()) {
    resolved_path = base::MakeAbsoluteFilePath(
        PathExists(resolved_path) ? resolved_path
                                  : test_data_path_.Append(resolved_path));
  }

  return PathExists(resolved_path)
             ? absl::optional<base::FilePath>(resolved_path)
             : absl::optional<base::FilePath>();
}

// static
void Video::DecodeTask(const std::vector<uint8_t> data,
                       const gfx::Size& resolution,
                       const size_t num_frames,
                       std::vector<uint8_t>* decompressed_data,
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

  // Setup the VP9 decoder.
  DecoderStatus init_result;
  VpxVideoDecoder decoder(
      media::OffloadableVideoDecoder::OffloadState::kOffloaded);
  media::VideoDecoder::InitCB init_cb =
      base::BindOnce([](DecoderStatus* save_to,
                        DecoderStatus save_from) { *save_to = save_from; },
                     &init_result);
  decoder.Initialize(config, false, nullptr, std::move(init_cb),
                     base::BindRepeating(&Video::OnFrameDecoded, resolution,
                                         decompressed_data),
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
      if (!*success)
        break;
      num_decoded_frames++;
    }
    av_packet_unref(&packet);
  }

  done->Signal();
}

// static
void Video::OnFrameDecoded(const gfx::Size& resolution,
                           std::vector<uint8_t>* data,
                           scoped_refptr<VideoFrame> frame) {
  ASSERT_EQ(frame->format(), VideoPixelFormat::PIXEL_FORMAT_I420);
  size_t num_planes = VideoFrame::NumPlanes(frame->format());
  // Copy the resolution area.
  for (size_t plane = 0; plane < num_planes; ++plane) {
    const int stride = frame->stride(plane);
    const int rows =
        VideoFrame::Rows(plane, frame->format(), resolution.height());
    const int row_bytes =
        VideoFrame::RowBytes(plane, frame->format(), resolution.width());
    // VideoFrame::PlaneSize() cannot be used because it computes the plane size
    // with resolutions aligned by two while our test code works with a succinct
    // buffer size.
    const int plane_size = row_bytes * rows;
    const size_t current_pos = data->size();
    // TODO(dstaessens): Avoid resizing.
    data->resize(data->size() + plane_size);
    uint8_t* dst = &data->at(current_pos);
    const uint8_t* src = frame->data(plane);
    libyuv::CopyPlane(src, stride, dst, row_bytes, row_bytes, rows);
  }
}

// static
absl::optional<VideoCodecProfile> Video::ConvertStringtoProfile(
    const std::string& profile) {
  if (profile == "H264PROFILE_BASELINE") {
    return H264PROFILE_BASELINE;
  } else if (profile == "H264PROFILE_MAIN") {
    return H264PROFILE_MAIN;
  } else if (profile == "H264PROFILE_HIGH") {
    return H264PROFILE_HIGH;
  } else if (profile == "VP8PROFILE_ANY") {
    return VP8PROFILE_ANY;
  } else if (profile == "VP9PROFILE_PROFILE0") {
    return VP9PROFILE_PROFILE0;
  } else if (profile == "VP9PROFILE_PROFILE2") {
    return VP9PROFILE_PROFILE2;
  } else if (profile == "AV1PROFILE_PROFILE_MAIN") {
    return AV1PROFILE_PROFILE_MAIN;
  } else if (profile == "HEVCPROFILE_MAIN") {
    return HEVCPROFILE_MAIN;
  } else if (profile == "HEVCPROFILE_MAIN10") {
    return HEVCPROFILE_MAIN10;
  } else {
    VLOG(2) << profile << " is not supported";
    return absl::nullopt;
  }
}

// static
absl::optional<VideoCodec> Video::ConvertProfileToCodec(
    VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    return VideoCodec::kH264;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    return VideoCodec::kVP8;
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return VideoCodec::kVP9;
  } else if (profile >= AV1PROFILE_MIN && profile <= AV1PROFILE_MAX) {
    return VideoCodec::kAV1;
  } else if (profile >= HEVCPROFILE_MIN && profile <= HEVCPROFILE_MAX) {
    return VideoCodec::kHEVC;
  } else {
    VLOG(2) << GetProfileName(profile) << " is not supported";
    return absl::nullopt;
  }
}

// static
absl::optional<VideoPixelFormat> Video::ConvertStringtoPixelFormat(
    const std::string& pixel_format) {
  if (pixel_format == "I420") {
    return VideoPixelFormat::PIXEL_FORMAT_I420;
  } else if (pixel_format == "NV12") {
    return VideoPixelFormat::PIXEL_FORMAT_NV12;
  } else {
    VLOG(2) << pixel_format << " is not supported";
    return absl::nullopt;
  }
}
}  // namespace test
}  // namespace media
