// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_metadata_sanitizer.h"

#include <algorithm>
#include <string>

#include "base/time/time.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace content {

namespace {

// Maximum length for all the strings inside the MediaMetadata when it is sent
// over IPC. The renderer process should truncate the strings before sending
// the MediaMetadata and the browser process must do the same when receiving
// it.
const size_t kMaxIPCStringLength = 4 * 1024;

// Maximum type length of MediaImage, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxMediaImageTypeLength = 2 * 127 + 1;

// Maximum number of MediaImages inside the MediaMetadata.
const size_t kMaxNumberOfMediaImages = 10;

// Maximum number of `ChapterInformation` inside the `MediaMetadata`.
const size_t kMaxNumberOfChapters = 200;

// Maximum of sizes in a MediaImage.
const size_t kMaxNumberOfMediaImageSizes = 10;

bool CheckMediaImageSrcSanity(const GURL& src) {
  if (!src.is_valid())
    return false;
  if (!src.SchemeIsHTTPOrHTTPS() &&
      !src.SchemeIs(url::kDataScheme) &&
      !src.SchemeIs(url::kBlobScheme))
    return false;
  if (src.spec().size() > url::kMaxURLChars)
    return false;

  return true;
}

bool CheckMediaImageSanity(const media_session::MediaImage& image) {
  if (!CheckMediaImageSrcSanity(image.src))
    return false;
  if (image.type.size() > kMaxMediaImageTypeLength)
    return false;
  if (image.sizes.size() > kMaxNumberOfMediaImageSizes)
    return false;

  return true;
}

bool CheckChapterInformationSanity(
    const media_session::ChapterInformation& chapter) {
  if (chapter.title().size() > kMaxIPCStringLength) {
    return false;
  }

  if (chapter.startTime() < base::Seconds(0)) {
    return false;
  }

  for (const auto& image : chapter.artwork()) {
    if (!CheckMediaImageSanity(image)) {
      return false;
    }
  }

  return true;
}

}  // anonymous namespace

bool MediaMetadataSanitizer::CheckSanity(
    const blink::mojom::SpecMediaMetadataPtr& metadata) {
  if (metadata->title.size() > kMaxIPCStringLength)
    return false;
  if (metadata->artist.size() > kMaxIPCStringLength)
    return false;
  if (metadata->album.size() > kMaxIPCStringLength)
    return false;
  if (metadata->artwork.size() > kMaxNumberOfMediaImages)
    return false;
  if (metadata->chapterInfo.size() > kMaxNumberOfChapters) {
    return false;
  }

  for (const auto& image : metadata->artwork) {
    if (!CheckMediaImageSanity(image))
      return false;
  }

  for (const auto& chapter : metadata->chapterInfo) {
    if (!CheckChapterInformationSanity(chapter)) {
      return false;
    }
  }

  return true;
}

}  // namespace content
