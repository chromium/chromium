// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/media_session/public/cpp/media_session_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/media_session/public/cpp/chapter_information.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media_session::mojom::MediaImageDataView,
                  media_session::MediaImage>::
    Read(media_session::mojom::MediaImageDataView data,
         media_session::MediaImage* out) {
  if (!data.ReadSrc(&out->src))
    return false;
  if (!data.ReadType(&out->type))
    return false;
  if (!data.ReadSizes(&out->sizes))
    return false;

  return true;
}

// static
bool StructTraits<media_session::mojom::MediaMetadataDataView,
                  media_session::MediaMetadata>::
    Read(media_session::mojom::MediaMetadataDataView data,
         media_session::MediaMetadata* out) {
  if (!data.ReadTitle(&out->title))
    return false;

  if (!data.ReadArtist(&out->artist))
    return false;

  if (!data.ReadAlbum(&out->album))
    return false;

  if (!data.ReadChapters(&out->chapters)) {
    return false;
  }

  if (!data.ReadSourceTitle(&out->source_title))
    return false;

  return true;
}

// static
const base::span<const uint8_t>
StructTraits<media_session::mojom::MediaImageBitmapDataView,
             SkBitmap>::pixel_data(const SkBitmap& r) {
  return base::make_span(static_cast<uint8_t*>(r.getPixels()),
                         r.computeByteSize());
}

// static
media_session::mojom::MediaImageBitmapColorType
StructTraits<media_session::mojom::MediaImageBitmapDataView,
             SkBitmap>::color_type(const SkBitmap& r) {
  switch (r.info().colorType()) {
    case (kRGBA_8888_SkColorType):
      return media_session::mojom::MediaImageBitmapColorType::kRGBA_8888;
    case (kBGRA_8888_SkColorType):
      return media_session::mojom::MediaImageBitmapColorType::kBGRA_8888;
    default:
      NOTREACHED_IN_MIGRATION();
      return media_session::mojom::MediaImageBitmapColorType::kRGBA_8888;
  }
}

// static
bool StructTraits<media_session::mojom::MediaImageBitmapDataView, SkBitmap>::
    Read(media_session::mojom::MediaImageBitmapDataView data, SkBitmap* out) {
  mojo::ArrayDataView<uint8_t> pixel_data;
  data.GetPixelDataDataView(&pixel_data);

  // Convert the mojo color type into the Skia equivalient. This will tell us
  // what format the image is in.
  SkColorType color_type;
  switch (data.color_type()) {
    case (media_session::mojom::MediaImageBitmapColorType::kRGBA_8888):
      color_type = kRGBA_8888_SkColorType;
      break;
    case (media_session::mojom::MediaImageBitmapColorType::kBGRA_8888):
      color_type = kBGRA_8888_SkColorType;
      break;
  }

  SkImageInfo info = SkImageInfo::Make(data.width(), data.height(), color_type,
                                       kPremul_SkAlphaType);
  if (info.computeByteSize(info.minRowBytes()) > pixel_data.size()) {
    // Insufficient buffer size.
    return false;
  }

  // Create the SkBitmap object which wraps the media image bitmap pixels.
  // This doesn't copy and |data| and |bitmap| share the buffer.
  SkBitmap bitmap;
  if (!bitmap.installPixels(info, const_cast<uint8_t*>(pixel_data.data()),
                            info.minRowBytes())) {
    // Error in installing pixels.
    return false;
  }

  // Copy the pixels into |out| and convert them to the system default color
  // type if needed.
  SkImageInfo out_info = info.makeColorType(kN32_SkColorType);
  return out->tryAllocPixels(out_info) &&
         bitmap.readPixels(out_info, out->getPixels(), out->rowBytes(), 0, 0);
}

// static
void StructTraits<media_session::mojom::MediaImageBitmapDataView,
                  SkBitmap>::SetToNull(SkBitmap* out) {
  out->reset();
}

// static
bool StructTraits<media_session::mojom::MediaPositionDataView,
                  media_session::MediaPosition>::
    Read(media_session::mojom::MediaPositionDataView data,
         media_session::MediaPosition* out) {
  if (!data.ReadDuration(&out->duration_))
    return false;

  if (!data.ReadPosition(&out->position_))
    return false;

  if (!data.ReadLastUpdatedTime(&out->last_updated_time_))
    return false;

  out->playback_rate_ = data.playback_rate();
  out->end_of_media_ = data.end_of_media();

  return true;
}

// static
bool StructTraits<media_session::mojom::ChapterInformationDataView,
                  media_session::ChapterInformation>::
    Read(media_session::mojom::ChapterInformationDataView data,
         media_session::ChapterInformation* out) {
  if (!data.ReadTitle(&out->title_)) {
    return false;
  }

  if (!data.ReadStartTime(&out->startTime_)) {
    return false;
  }

  if (!data.ReadArtwork(&out->artwork_)) {
    return false;
  }

  return true;
}

}  // namespace mojo
