// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "services/media_session/public/cpp/chapter_information.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<media_session::mojom::MediaImageDataView,
                    media_session::MediaImage> {
  static const GURL& src(const media_session::MediaImage& image) {
    return image.src;
  }

  static const std::u16string& type(const media_session::MediaImage& image) {
    return image.type;
  }

  static const std::vector<gfx::Size>& sizes(
      const media_session::MediaImage& image) {
    return image.sizes;
  }

  static bool Read(media_session::mojom::MediaImageDataView data,
                   media_session::MediaImage* out);
};

template <>
struct StructTraits<media_session::mojom::MediaMetadataDataView,
                    media_session::MediaMetadata> {
  static const std::u16string& title(
      const media_session::MediaMetadata& metadata) {
    return metadata.title;
  }

  static const std::u16string& artist(
      const media_session::MediaMetadata& metadata) {
    return metadata.artist;
  }

  static const std::u16string& album(
      const media_session::MediaMetadata& metadata) {
    return metadata.album;
  }

  static const std::vector<media_session::ChapterInformation>& chapters(
      const media_session::MediaMetadata& metadata) {
    return metadata.chapters;
  }

  static const std::u16string& source_title(
      const media_session::MediaMetadata& metadata) {
    return metadata.source_title;
  }

  static bool Read(media_session::mojom::MediaMetadataDataView data,
                   media_session::MediaMetadata* out);
};

// TODO(beccahughes): de-dupe this with ArcBitmap.
template <>
struct StructTraits<media_session::mojom::MediaImageBitmapDataView, SkBitmap> {
  static const base::span<const uint8_t> pixel_data(const SkBitmap& r);
  static int width(const SkBitmap& r) { return r.width(); }
  static int height(const SkBitmap& r) { return r.height(); }
  static media_session::mojom::MediaImageBitmapColorType color_type(
      const SkBitmap& r);

  static bool Read(media_session::mojom::MediaImageBitmapDataView data,
                   SkBitmap* out);

  static bool IsNull(const SkBitmap& r) { return r.isNull(); }
  static void SetToNull(SkBitmap* out);
};

template <>
struct StructTraits<media_session::mojom::MediaPositionDataView,
                    media_session::MediaPosition> {
  static double playback_rate(
      const media_session::MediaPosition& media_position) {
    return media_position.playback_rate_;
  }

  static base::TimeDelta duration(
      const media_session::MediaPosition& media_position) {
    return media_position.duration_;
  }

  static base::TimeDelta position(
      const media_session::MediaPosition& media_position) {
    return media_position.position_;
  }

  static base::TimeTicks last_updated_time(
      const media_session::MediaPosition& media_position) {
    return media_position.last_updated_time_;
  }

  static bool end_of_media(const media_session::MediaPosition& media_position) {
    return media_position.end_of_media_;
  }

  static bool Read(media_session::mojom::MediaPositionDataView data,
                   media_session::MediaPosition* out);
};

template <>
struct StructTraits<media_session::mojom::ChapterInformationDataView,
                    media_session::ChapterInformation> {
  static const std::u16string& title(
      const media_session::ChapterInformation& chapter_information) {
    return chapter_information.title_;
  }

  static base::TimeDelta startTime(
      const media_session::ChapterInformation& chapter_information) {
    return chapter_information.startTime_;
  }

  static const std::vector<media_session::MediaImage>& artwork(
      const media_session::ChapterInformation& chapter_information) {
    return chapter_information.artwork_;
  }

  static bool Read(media_session::mojom::ChapterInformationDataView data,
                   media_session::ChapterInformation* out);
};

}  // namespace mojo

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_
