// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/image.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "media/base/test_data_util.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

namespace {

// Resolve the specified test file path to an absolute path. The path can be
// either an absolute path, a path relative to the current directory, or a path
// relative to the test data path.
void ResolveTestFilePath(base::FilePath* file_path) {
  if (!file_path->IsAbsolute()) {
    if (!PathExists(*file_path))
      *file_path = media::GetTestDataPath().Append(*file_path);
    *file_path = base::MakeAbsoluteFilePath(*file_path);
  }
}

// Converts the |pixel_format| string into a VideoPixelFormat.
VideoPixelFormat ConvertStringtoPixelFormat(const std::string& pixel_format) {
  if (pixel_format == "BGRA") {
    return PIXEL_FORMAT_ARGB;
  } else if (pixel_format == "I420") {
    return PIXEL_FORMAT_I420;
  } else if (pixel_format == "NV12") {
    return PIXEL_FORMAT_NV12;
  } else if (pixel_format == "YV12") {
    return PIXEL_FORMAT_YV12;
  } else if (pixel_format == "RGBA") {
    return PIXEL_FORMAT_ABGR;
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

  ResolveTestFilePath(&file_path_);

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
  base::MD5Sum(mapped_file_.data(), mapped_file_.length(), &digest);
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
  ResolveTestFilePath(&json_path);

  if (!base::PathExists(json_path)) {
    VLOGF(1) << "Image metadata file not found: " << json_path.BaseName();
    return false;
  }

  std::string json_data;
  if (!base::ReadFileToString(json_path, &json_data)) {
    VLOGF(1) << "Failed to read image metadata file: " << json_path;
    return false;
  }

  base::JSONReader reader;
  std::unique_ptr<base::Value> metadata(
      reader.ReadToValueDeprecated(json_data));
  if (!metadata) {
    VLOGF(1) << "Failed to parse image metadata: " << json_path << ": "
             << reader.GetErrorMessage();
    return false;
  }

  // Get the pixel format from the json data.
  const base::Value* pixel_format =
      metadata->FindKeyOfType("pixel_format", base::Value::Type::STRING);
  if (!pixel_format) {
    VLOGF(1) << "Key \"pixel_format\" is not found in " << json_path;
    return false;
  }
  pixel_format_ = ConvertStringtoPixelFormat(pixel_format->GetString());
  if (pixel_format_ == PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << pixel_format->GetString() << " is not supported";
    return false;
  }

  // Get the image dimensions from the json data.
  const base::Value* width =
      metadata->FindKeyOfType("width", base::Value::Type::INTEGER);
  if (!width) {
    VLOGF(1) << "Key \"width\" is not found in " << json_path;
    return false;
  }
  const base::Value* height =
      metadata->FindKeyOfType("height", base::Value::Type::INTEGER);
  if (!height) {
    VLOGF(1) << "Key \"height\" is not found in " << json_path;
    return false;
  }
  size_ = gfx::Size(width->GetInt(), height->GetInt());

  // Get the image checksum from the json data.
  const base::Value* checksum =
      metadata->FindKeyOfType("checksum", base::Value::Type::STRING);
  if (!checksum) {
    VLOGF(1) << "Key \"checksum\" is not found in " << json_path;
    return false;
  }
  checksum_ = checksum->GetString();

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

const char* Image::Checksum() const {
  return checksum_.data();
}

}  // namespace test
}  // namespace media
