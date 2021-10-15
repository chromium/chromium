// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_STABLE_STABLE_VIDEO_DECODER_TYPES_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STABLE_STABLE_VIDEO_DECODER_TYPES_MOJOM_TRAITS_H_

#include "media/mojo/mojom/stable/stable_video_decoder_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media::stable::mojom::VideoCodec, ::media::VideoCodec> {
  static media::stable::mojom::VideoCodec ToMojom(::media::VideoCodec input) {
    switch (input) {
      case ::media::VideoCodec::kUnknown:
        return media::stable::mojom::VideoCodec::kUnknown;
      case ::media::VideoCodec::kH264:
        return media::stable::mojom::VideoCodec::kH264;
      case ::media::VideoCodec::kVC1:
        return media::stable::mojom::VideoCodec::kVC1;
      case ::media::VideoCodec::kMPEG2:
        return media::stable::mojom::VideoCodec::kMPEG2;
      case ::media::VideoCodec::kMPEG4:
        return media::stable::mojom::VideoCodec::kMPEG4;
      case ::media::VideoCodec::kTheora:
        return media::stable::mojom::VideoCodec::kTheora;
      case ::media::VideoCodec::kVP8:
        return media::stable::mojom::VideoCodec::kVP8;
      case ::media::VideoCodec::kVP9:
        return media::stable::mojom::VideoCodec::kVP9;
      case ::media::VideoCodec::kHEVC:
        return media::stable::mojom::VideoCodec::kHEVC;
      case ::media::VideoCodec::kDolbyVision:
        return media::stable::mojom::VideoCodec::kDolbyVision;
      case ::media::VideoCodec::kAV1:
        return media::stable::mojom::VideoCodec::kAV1;
    }

    NOTREACHED();
    return media::stable::mojom::VideoCodec::kUnknown;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::stable::mojom::VideoCodec input,
                        media::VideoCodec* output) {
    switch (input) {
      case media::stable::mojom::VideoCodec::kUnknown:
        *output = ::media::VideoCodec::kUnknown;
        return true;
      case media::stable::mojom::VideoCodec::kH264:
        *output = ::media::VideoCodec::kH264;
        return true;
      case media::stable::mojom::VideoCodec::kVC1:
        *output = ::media::VideoCodec::kVC1;
        return true;
      case media::stable::mojom::VideoCodec::kMPEG2:
        *output = ::media::VideoCodec::kMPEG2;
        return true;
      case media::stable::mojom::VideoCodec::kMPEG4:
        *output = ::media::VideoCodec::kMPEG4;
        return true;
      case media::stable::mojom::VideoCodec::kTheora:
        *output = ::media::VideoCodec::kTheora;
        return true;
      case media::stable::mojom::VideoCodec::kVP8:
        *output = ::media::VideoCodec::kVP8;
        return true;
      case media::stable::mojom::VideoCodec::kVP9:
        *output = ::media::VideoCodec::kVP9;
        return true;
      case media::stable::mojom::VideoCodec::kHEVC:
        *output = ::media::VideoCodec::kHEVC;
        return true;
      case media::stable::mojom::VideoCodec::kDolbyVision:
        *output = ::media::VideoCodec::kDolbyVision;
        return true;
      case media::stable::mojom::VideoCodec::kAV1:
        *output = ::media::VideoCodec::kAV1;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<media::stable::mojom::SubsampleEntryDataView,
                    ::media::SubsampleEntry> {
  static uint32_t clear_bytes(const ::media::SubsampleEntry& input);

  static uint32_t cypher_bytes(const ::media::SubsampleEntry& input);

  static bool Read(media::stable::mojom::SubsampleEntryDataView input,
                   ::media::SubsampleEntry* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STABLE_STABLE_VIDEO_DECODER_TYPES_MOJOM_TRAITS_H_
