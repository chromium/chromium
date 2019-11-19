// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

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

bool Video::Load() {
  // TODO(dstaessens@) Investigate reusing existing infrastructure such as
  //                   DecoderBuffer.
  DCHECK(!file_path_.empty());
  DCHECK(data_.empty());

  base::Optional<base::FilePath> resolved_path = ResolveFilePath(file_path_);
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

VideoCodec Video::Codec() const {
  return codec_;
}

VideoCodecProfile Video::Profile() const {
  return profile_;
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

base::TimeDelta Video::GetDuration() const {
  return base::TimeDelta::FromSecondsD(static_cast<double>(num_frames_) /
                                       static_cast<double>(frame_rate_));
}

const std::vector<std::string>& Video::FrameChecksums() const {
  return frame_checksums_;
}

const std::vector<std::string>& Video::ThumbnailChecksums() const {
  return thumbnail_checksums_;
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

  base::Optional<base::FilePath> resolved_path =
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

  base::JSONReader reader;
  std::unique_ptr<base::Value> metadata(
      reader.ReadToValueDeprecated(json_data));
  if (!metadata) {
    LOG(ERROR) << "Failed to parse video metadata: " << metadata_file_path_
               << ": " << reader.GetErrorMessage();
    return false;
  }

  const base::Value* profile =
      metadata->FindKeyOfType("profile", base::Value::Type::STRING);
  if (!profile) {
    LOG(ERROR) << "Key \"profile\" is not found in " << metadata_file_path_;
    return false;
  }
  profile_ = ConvertStringtoProfile(profile->GetString());
  codec_ = ConvertProfileToCodec(profile_);
  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN || codec_ == kUnknownVideoCodec) {
    LOG(ERROR) << profile->GetString() << " is not supported";
    return false;
  }

  const base::Value* frame_rate =
      metadata->FindKeyOfType("frame_rate", base::Value::Type::INTEGER);
  if (!frame_rate) {
    LOG(ERROR) << "Key \"frame_rate\" is not found in " << metadata_file_path_;
    return false;
  }
  frame_rate_ = static_cast<uint32_t>(frame_rate->GetInt());

  const base::Value* num_frames =
      metadata->FindKeyOfType("num_frames", base::Value::Type::INTEGER);
  if (!num_frames) {
    LOG(ERROR) << "Key \"num_frames\" is not found in " << metadata_file_path_;
    return false;
  }
  num_frames_ = static_cast<uint32_t>(num_frames->GetInt());

  const base::Value* num_fragments =
      metadata->FindKeyOfType("num_fragments", base::Value::Type::INTEGER);
  if (!num_fragments) {
    LOG(ERROR) << "Key \"num_fragments\" is not found in "
               << metadata_file_path_;
    return false;
  }
  num_fragments_ = static_cast<uint32_t>(num_fragments->GetInt());

  const base::Value* width =
      metadata->FindKeyOfType("width", base::Value::Type::INTEGER);
  if (!width) {
    LOG(ERROR) << "Key \"width\" is not found in " << metadata_file_path_;
    return false;
  }
  const base::Value* height =
      metadata->FindKeyOfType("height", base::Value::Type::INTEGER);
  if (!height) {
    LOG(ERROR) << "Key \"height\" is not found in " << metadata_file_path_;
    return false;
  }
  resolution_ = gfx::Size(static_cast<uint32_t>(width->GetInt()),
                          static_cast<uint32_t>(height->GetInt()));

  const base::Value* md5_checksums =
      metadata->FindKeyOfType("md5_checksums", base::Value::Type::LIST);
  if (!md5_checksums) {
    LOG(ERROR) << "Key \"md5_checksums\" is not found in "
               << metadata_file_path_;
    return false;
  }
  for (const base::Value& checksum : md5_checksums->GetList()) {
    frame_checksums_.push_back(checksum.GetString());
  }

  const base::Value* thumbnail_checksums =
      metadata->FindKeyOfType("thumbnail_checksums", base::Value::Type::LIST);
  if (!thumbnail_checksums) {
    LOG(ERROR) << "Key \"thumbnail_checksums\" is not found in "
               << metadata_file_path_;
    return false;
  }
  for (const base::Value& checksum : thumbnail_checksums->GetList()) {
    const std::string& checksum_str = checksum.GetString();
    if (checksum_str.size() > 0 && checksum_str[0] != '#')
      thumbnail_checksums_.push_back(checksum_str);
  }

  return true;
}

bool Video::IsMetadataLoaded() const {
  return profile_ != VIDEO_CODEC_PROFILE_UNKNOWN || num_frames_ != 0;
}

base::Optional<base::FilePath> Video::ResolveFilePath(
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
             ? base::Optional<base::FilePath>(resolved_path)
             : base::Optional<base::FilePath>();
}

// static
VideoCodecProfile Video::ConvertStringtoProfile(const std::string& profile) {
  if (profile == "H264PROFILE_MAIN") {
    return H264PROFILE_MAIN;
  } else if (profile == "VP8PROFILE_ANY") {
    return VP8PROFILE_ANY;
  } else if (profile == "VP9PROFILE_PROFILE0") {
    return VP9PROFILE_PROFILE0;
  } else if (profile == "VP9PROFILE_PROFILE2") {
    return VP9PROFILE_PROFILE2;
  } else {
    VLOG(2) << profile << " is not supported";
    return VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

// static
VideoCodec Video::ConvertProfileToCodec(VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    return kCodecH264;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    return kCodecVP8;
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return kCodecVP9;
  } else {
    VLOG(2) << GetProfileName(profile) << " is not supported";
    return kUnknownVideoCodec;
  }
}

}  // namespace test
}  // namespace media
