// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CHANNEL_LAYOUT_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CHANNEL_LAYOUT_MOJOM_TRAITS_H_

#include "media/base/channel_layout.h"
#include "media/mojo/mojom/channel_layout.mojom-shared.h"
#include "mojo/public/cpp/base/empty_mojom_traits.h"

namespace mojo {

template <>
struct UnionTraits<media::mojom::ChannelLayoutConfigDataView,
                   media::ChannelLayoutConfig> {
  static media::mojom::ChannelLayoutConfigDataView::Tag GetTag(
      const media::ChannelLayoutConfig& input);

  static media::mojom::ChannelLayout predefined_layout(
      const media::ChannelLayoutConfig& input);
  static uint32_t discrete_channels(const media::ChannelLayoutConfig& input);

  static bool Read(media::mojom::ChannelLayoutConfigDataView input,
                   media::ChannelLayoutConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CHANNEL_LAYOUT_MOJOM_TRAITS_H_
