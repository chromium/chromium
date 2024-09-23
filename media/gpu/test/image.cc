// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/image.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "media/base/test_data_util.h"
#include "media/gpu/macros.h"

namespace media {
namespace test {

namespace {

// Resolve the specified test file path to an absolute path. The path can be
// either an absolute path, a path relative to the current directory, or a path
// relative to the test data path.
std::optional<base::FilePath> ResolveFilePath(const base::FilePath& file_path) {
  base::FilePath resolved_path = file_path;

  // Try to resolve the path into an absolute path. If the path doesn't exist,
  // it might be relative to the test data dir.
  if (!resolved_path.IsAbsolute()) {
    resolved_path = base::MakeAbsoluteFilePath(
        PathExists(resolved_path)
            ? resolved_path
            : media::GetTestDataPath().Append(resolved_path));
  }

  return PathExists(resolved_path)
             ? std::optional<base::FilePath>(resolved_path)
             : std::nullopt;
}

// Converts the |pixel_format| string into a VideoPixelFormat.
VideoPixelFormat ConvertStringtoPixelFormat(const std::string& pixel_format) {
  if (pixel_format == "BGRA") {
    return PIXEL_FORMAT_ARGB;
  } else if (pixel_format == "I420") {
    return PIXEL_FORMAT_I420;
  } else if (pixel_format == "NV12") {
    return PIXEL_FORMAT_NV12;
  } else if (pixel_format == "P010" || pixel_format == "MT2T") {
    return PIXEL_FORMAT_P010LE;
  } else if (pixel_format == "YV12") {
    return PIXEL_FORMAT_YV12;
  } else if (pixel_format == "RGBA") {
    return PIXEL_FORMAT_ABGR;
  } else if (pixel_format == "I422") {
    return PIXEL_FORMAT_I422;
  } else if (pixel_format == "YUYV") {
    return PIXEL_FORMAT_YUY2;
  } else {
    VLOG(2) << pixel_format << " is not supported.";
    return PIXEL_FORMAT_UNKNOWN;
  }
}

}  // namespace

// Suffix to append to the image file path to get the metadata file path.
constexpr const base::FilePath::CharType* kMetadataSuffix =
    FILE_PATH_LITERAL(".json");

Image::Image(const base::FilePath& file_path) : file_path_(file_path) {}

Image::~Image() {}

bool Image::Load() {
  DCHECK(!file_path_.empty());
  DCHECK(!IsLoaded());

  std::optional<base::FilePath> resolved_path = ResolveFilePath(file_path_);
  if (!resolved_path) {
    LOG(ERROR) << "Image file not found: " << file_path_;
    return false;
  }
  file_path_ = resolved_path.value();
  DVLOGF(2) << "File path: " << file_path_;

  if (!mapped_file_.Initialize(file_path_)) {
    LOG(ERROR) << "Failed to read file: " << file_path_;
    return false;
  }

  if (!LoadMetadata()) {
    LOG(ERROR) << "Failed to load metadata";
    return false;
  }

  // Verify that the image's checksum matches the checksum in the metadata.
  base::MD5Digest digest;
  base::MD5Sum(mapped_file_.bytes(), &digest);
  if (base::MD5DigestToBase16(digest) != checksum_) {
    LOG(ERROR) << "Image checksum not matching metadata";
    return false;
  }

  return true;
}

bool Image::IsLoaded() const {
  return mapped_file_.IsValid();
}

bool Image::LoadMetadata() {
  if (IsMetadataLoaded()) {
    return true;
  }

  base::FilePath json_path = file_path_.AddExtension(kMetadataSuffix);
  std::optional<base::FilePath> resolved_path = ResolveFilePath(json_path);
  if (!resolved_path) {
    LOG(ERROR) << "Image metadata file not found: " << json_path;
    return false;
  }
  json_path = resolved_path.value();

  if (!base::PathExists(json_path)) {
    VLOGF(1) << "Image metadata file not found: " << json_path.BaseName();
    return false;
  }

  std::string json_data;
  if (!base::ReadFileToString(json_path, &json_data)) {
    VLOGF(1) << "Failed to read image metadata file: " << json_path;
    return false;
  }

  auto metadata_result =
      base::JSONReader::ReadAndReturnValueWithError(json_data);
  if (!metadata_result.has_value()) {
    VLOGF(1) << "Failed to parse image metadata: " << json_path << ": "
             << metadata_result.error().message;
    return false;
  }
  const base::Value::Dict& metadata = metadata_result->GetDict();

  // Get the pixel format from the json data.
  const std::string* pixel_format = metadata.FindString("pixel_format");
  if (!pixel_format) {
    VLOGF(1) << "Key \"pixel_format\" is not found in " << json_path;
    return false;
  }
  pixel_format_ = ConvertStringtoPixelFormat(*pixel_format);
  if (pixel_format_ == PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << *pixel_format << " is not supported";
    return false;
  }

  // Get the image dimensions from the json data.
  std::optional<int> width = metadata.FindInt("width");
  if (!width.has_value()) {
    VLOGF(1) << "Key \"width\" is not found in " << json_path;
    return false;
  }
  std::optional<int> height = metadata.FindInt("height");
  if (!height) {
    VLOGF(1) << "Key \"height\" is not found in " << json_path;
    return false;
  }
  size_ = gfx::Size(*width, *height);

  // Try to get the visible rectangle of the image from the json data.
  // These values are not in json data if all the image data is in the visible
  // area.
  visible_rect_ = gfx::Rect(size_);
  const base::Value::List* visible_rect_info =
      metadata.FindList("visible_rect");
  if (visible_rect_info) {
    const base::Value::List& values = *visible_rect_info;
    if (values.size() != 4) {
      VLOGF(1) << "unexpected json format for visible rectangle";
      return false;
    }
    int origin_x = values[0].GetInt();
    int origin_y = values[1].GetInt();
    int visible_width = values[2].GetInt();
    int visible_height = values[3].GetInt();
    visible_rect_ =
        gfx::Rect(origin_x, origin_y, visible_width, visible_height);
  }

  // Get the image rotation info from the json data.
  std::optional<int> rotation = metadata.FindInt("rotation");
  if (!rotation.has_value()) {
    // Default rotation value is VIDEO_ROTATION_0
    rotation_ = VIDEO_ROTATION_0;
  } else {
    switch (*rotation) {
      case 0:
        rotation_ = VIDEO_ROTATION_0;
        break;
      case 90:
        rotation_ = VIDEO_ROTATION_90;
        break;
      case 180:
        rotation_ = VIDEO_ROTATION_180;
        break;
      case 270:
        rotation_ = VIDEO_ROTATION_270;
        break;
      default:
        VLOGF(1) << "Invalid rotation value: " << *rotation;
        return false;
    };
  }

  // Get the image checksum from the json data.
  const std::string* checksum = metadata.FindString("checksum");
  if (!checksum) {
    VLOGF(1) << "Key \"checksum\" is not found in " << json_path;
    return false;
  }
  checksum_ = *checksum;

  return true;
}

bool Image::IsMetadataLoaded() const {
  return pixel_format_ != PIXEL_FORMAT_UNKNOWN;
}

uint8_t* Image::Data() const {
  return mapped_file_.data();
}

size_t Image::DataSize() const {
  return mapped_file_.length();
}

VideoPixelFormat Image::PixelFormat() const {
  return pixel_format_;
}

const gfx::Size& Image::Size() const {
  return size_;
}

const gfx::Rect& Image::VisibleRect() const {
  return visible_rect_;
}

VideoRotation Image::Rotation() const {
  return rotation_;
}

const char* Image::Checksum() const {
  return checksum_.data();
}

}  // namespace test
}  // namespace media
