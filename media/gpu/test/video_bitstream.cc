// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_bitstream.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"

namespace media::test {

namespace {
// Suffix appended to the video file path to get the metadata file path, if no
// explicit metadata file path was specified.
constexpr const base::FilePath::CharType* kMetadataSuffix =
    FILE_PATH_LITERAL(".json");

// Converts the string to VideoCodecProfile. This returns std::nullopt if
// it is not supported by video_decode_accelerator(_perf)_tests.
std::optional<VideoCodecProfile> ConvertStringtoProfile(
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
    LOG(ERROR) << profile << " is not supported";
    return std::nullopt;
  }
}

// Loads the compressed video from |data_file_path|.
std::unique_ptr<base::MemoryMappedFile> LoadData(
    const base::FilePath& data_file_path) {
  auto memory_mapped_file = std::make_unique<base::MemoryMappedFile>();
  if (!memory_mapped_file->Initialize(data_file_path,
                                      base::MemoryMappedFile::READ_ONLY)) {
    LOG(ERROR) << "Failed to read the file: " << data_file_path;
    return nullptr;
  }
  return memory_mapped_file;
}
}  // namespace

// Loads the metadata from |json_file_path|. The read values are filled into
// |metadata.|
// static
bool VideoBitstream::LoadMetadata(const base::FilePath& json_file_path,
                                  Metadata& metadata) {
  std::string json_data;
  if (!base::ReadFileToString(json_file_path, &json_data)) {
    return false;
  }
  auto metadata_result =
      base::JSONReader::ReadAndReturnValueWithError(json_data);
  if (!metadata_result.has_value()) {
    LOG(ERROR) << "Failed to parse video metadata: " << json_file_path << ": "
               << metadata_result.error().message;
    return false;
  }
  const base::Value::Dict& metadata_dict = metadata_result->GetDict();

  const std::string* profile = metadata_dict.FindString("profile");
  auto converted_profile = ConvertStringtoProfile(*profile);
  if (!converted_profile) {
    LOG(ERROR) << *profile << " is not supported";
    return false;
  }
  metadata.profile = converted_profile.value();
  metadata.codec = VideoCodecProfileToVideoCodec(metadata.profile);
  CHECK_NE(metadata.codec, VideoCodec::kUnknown);

  // Find the video's bit depth. This is optional.
  std::optional<int> bit_depth = metadata_dict.FindInt("bit_depth");
  if (bit_depth.has_value()) {
    metadata.bit_depth = base::checked_cast<uint8_t>(*bit_depth);
  } else {
    if (metadata.profile == VP9PROFILE_PROFILE2) {
      LOG(ERROR) << "Bit depth is unspecified for VP9 profile 2";
      return false;
    }
    constexpr uint8_t kDefaultBitDepth = 8u;
    metadata.bit_depth = kDefaultBitDepth;
  }

  std::optional<int> frame_rate = metadata_dict.FindInt("frame_rate");
  if (!frame_rate.has_value()) {
    LOG(ERROR) << "Key \"frame_rate\" is not found in " << json_file_path;
    return false;
  }
  metadata.frame_rate = base::checked_cast<uint32_t>(*frame_rate);

  std::optional<int> num_frames = metadata_dict.FindInt("num_frames");
  if (!num_frames.has_value()) {
    LOG(ERROR) << "Key \"num_frames\" is not found in " << json_file_path;
    return false;
  }
  metadata.num_frames = base::checked_cast<size_t>(*num_frames);

  std::optional<int> width = metadata_dict.FindInt("width");
  if (!width.has_value()) {
    LOG(ERROR) << "Key \"width\" is not found in " << json_file_path;
    return false;
  }
  std::optional<int> height = metadata_dict.FindInt("height");
  if (!height) {
    LOG(ERROR) << "Key \"height\" is not found in " << json_file_path;
    return false;
  }
  metadata.resolution =
      gfx::Size(static_cast<uint32_t>(*width), static_cast<uint32_t>(*height));

  const base::Value::List* md5_checksums =
      metadata_dict.FindList("md5_checksums");
  for (const base::Value& checksum : *md5_checksums) {
    metadata.frame_checksums.push_back(checksum.GetString());
  }

  return true;
}

VideoBitstream::VideoBitstream(
    std::unique_ptr<base::MemoryMappedFile> memory_mapped_file,
    const Metadata& metadata)
    : memory_mapped_file_(std::move(memory_mapped_file)), metadata_(metadata) {}

VideoBitstream::~VideoBitstream() = default;

VideoBitstream::Metadata::Metadata() = default;
VideoBitstream::Metadata::~Metadata() = default;
VideoBitstream::Metadata::Metadata(const Metadata&) = default;
VideoBitstream::Metadata& VideoBitstream::Metadata::operator=(const Metadata&) =
    default;

std::unique_ptr<VideoBitstream> VideoBitstream::Create(
    const base::FilePath& file_path,
    const base::FilePath& metadata_file_path) {
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

  auto memory_mapped_file = LoadData(data_file_path);
  if (!memory_mapped_file) {
    return nullptr;
  }

  VideoBitstream::Metadata metadata;
  if (!LoadMetadata(json_file_path, metadata)) {
    LOG(ERROR) << "Failed to read metadata file: " << data_file_path;
    return nullptr;
  }
  // We set |has_keyframeless_resolution_change| by looking at the file name.
  base::FilePath kKeyFrameLessResolutionChangeFiles[] = {
      base::FilePath::FromASCII("frm_resize"),
      base::FilePath::FromASCII("sub8x8_sf"),
  };
  metadata.has_keyframeless_resolution_change =
      std::find_if(std::cbegin(kKeyFrameLessResolutionChangeFiles),
                   std::cend(kKeyFrameLessResolutionChangeFiles),
                   [filepath = data_file_path.value()](base::FilePath substr) {
                     return base::Contains(base::ToLowerASCII(filepath),
                                           base::ToLowerASCII(substr.value()));
                   });
  return base::WrapUnique(
      new VideoBitstream(std::move(memory_mapped_file), metadata));
}

base::span<const uint8_t> VideoBitstream::Data() const {
  CHECK(memory_mapped_file_ && memory_mapped_file_->IsValid());
  return base::span<const uint8_t>(memory_mapped_file_->data(),
                                   memory_mapped_file_->length());
}
// static
base::FilePath VideoBitstream::test_data_path_;

// static
void VideoBitstream::SetTestDataPath(const base::FilePath& test_data_path) {
  test_data_path_ = test_data_path;
}

// static
base::FilePath VideoBitstream::ResolveFilePath(
    const base::FilePath& file_path) {
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
