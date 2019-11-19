// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_image_manager.h"

#include <algorithm>

#include "base/hash/hash.h"
#include "ui/gfx/geometry/size.h"

namespace media_session {

namespace {

// The default score of unknown image size.
const double kDefaultImageSizeScore = 0.4;

// The scores for different image types. Keep them sorted by value.
const double kDefaultTypeScore = 0.6;
const double kPNGTypeScore = 1.0;
const double kJPEGTypeScore = 0.7;
const double kBMPTypeScore = 0.5;
const double kXIconTypeScore = 0.4;
const double kGIFTypeScore = 0.3;

double GetImageAspectRatioScore(const gfx::Size& size) {
  double long_edge = std::max(size.width(), size.height());
  double short_edge = std::min(size.width(), size.height());
  return short_edge / long_edge;
}

std::string GetExtension(const std::string& path) {
  auto const pos = path.find_last_of('.');
  if (pos == std::string::npos)
    return std::string();
  return base::ToLowerASCII(path.substr(pos));
}

double GetImageDominantSizeScore(int min_size,
                                 int ideal_size,
                                 const gfx::Size& size) {
  int dominant_size = std::max(size.width(), size.height());

  // If the size is "any".
  if (dominant_size == 0)
    return 0.8;

  // Ignore images that are too small.
  if (dominant_size < min_size)
    return 0;

  if (dominant_size < ideal_size)
    return 0.8 * (dominant_size - min_size) / (ideal_size - min_size) + 0.2;

  return 1.0 * ideal_size / dominant_size;
}

}  // namespace

// static
double MediaImageManager::GetImageSizeScore(int min_size,
                                            int ideal_size,
                                            const gfx::Size& size) {
  return GetImageDominantSizeScore(min_size, ideal_size, size) *
         GetImageAspectRatioScore(size);
}

MediaImageManager::MediaImageManager(int min_size, int ideal_size)
    : min_size_(min_size), ideal_size_(ideal_size) {}

MediaImageManager::~MediaImageManager() = default;

base::Optional<MediaImage> MediaImageManager::SelectImage(
    const std::vector<MediaImage>& images) {
  base::Optional<MediaImage> selected;

  double best_score = 0;
  for (auto& image : images) {
    double score = GetImageScore(image);
    if (score > best_score) {
      best_score = score;
      selected = image;
    }
  }

  return selected;
}

double MediaImageManager::GetImageScore(const MediaImage& image) const {
  double best_size_score = 0;

  if (image.sizes.empty()) {
    best_size_score = kDefaultImageSizeScore;
  } else {
    for (auto& size : image.sizes) {
      best_size_score = std::max(
          best_size_score, GetImageSizeScore(min_size_, ideal_size_, size));
    }
  }

  double type_score = kDefaultTypeScore;
  if (base::Optional<double> ext_score = GetImageExtensionScore(image.src)) {
    type_score = *ext_score;
  } else if (base::Optional<double> mime_score =
                 GetImageTypeScore(image.type)) {
    type_score = *mime_score;
  }

  return best_size_score * type_score;
}

// static
base::Optional<double> MediaImageManager::GetImageExtensionScore(
    const GURL& url) {
  if (!url.has_path())
    return base::nullopt;

  std::string extension = GetExtension(url.path());

  // These hashes are calculated in
  // MediaImageManagerTest_CheckExpectedImageExtensionHashes
  switch (base::PersistentHash(extension)) {
    case 0x17f3f565:  // .png
      return kPNGTypeScore;
    case 0x32937444:  // .jpeg
      return kJPEGTypeScore;
    case 0x384e2a41:  // .jpg
      return kJPEGTypeScore;
    case 0x9e9716f8:  // .bmp
      return kBMPTypeScore;
    case 0xa123ca62:  // .icon
      return kXIconTypeScore;
    case 0x97112590:  // .gif
      return kGIFTypeScore;
  }

  return base::nullopt;
}

// static
base::Optional<double> MediaImageManager::GetImageTypeScore(
    const base::string16& type) {
  // These hashes are calculated in
  // MediaImageManagerTest_CheckExpectedImageTypeHashes
  switch (
      base::PersistentHash(type.data(), type.size() * sizeof(base::char16))) {
    case 0xfd295465:  // image/bmp
      return kBMPTypeScore;
    case 0xce81e113:  // image/gif
      return kGIFTypeScore;
    case 0xb1b44900:  // image/jpeg
      return kJPEGTypeScore;
    case 0x466b4956:  // image/png
      return kPNGTypeScore;
    case 0x5668ffa3:  // image/x-icon
      return kXIconTypeScore;
  }

  return base::nullopt;
}

}  // namespace media_session
