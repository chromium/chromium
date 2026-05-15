// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/channel_layout_mojom_traits.h"

#include "media/mojo/mojom/media_types_enum_mojom_traits.h"

namespace mojo {

// static
media::mojom::ChannelLayoutConfigDataView::Tag UnionTraits<
    media::mojom::ChannelLayoutConfigDataView,
    media::ChannelLayoutConfig>::GetTag(const media::ChannelLayoutConfig&
                                            input) {
  if (input.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE) {
    return media::mojom::ChannelLayoutConfigDataView::Tag::kDiscreteChannels;
  }
  return media::mojom::ChannelLayoutConfigDataView::Tag::kPredefinedLayout;
}

// static
media::mojom::ChannelLayout
UnionTraits<media::mojom::ChannelLayoutConfigDataView,
            media::ChannelLayoutConfig>::
    predefined_layout(const media::ChannelLayoutConfig& input) {
  DCHECK_NE(input.channel_layout(), media::CHANNEL_LAYOUT_DISCRETE);
  return EnumTraits<media::mojom::ChannelLayout, media::ChannelLayout>::ToMojom(
      input.channel_layout());
}

// static
uint32_t UnionTraits<media::mojom::ChannelLayoutConfigDataView,
                     media::ChannelLayoutConfig>::
    discrete_channels(const media::ChannelLayoutConfig& input) {
  DCHECK_EQ(input.channel_layout(), media::CHANNEL_LAYOUT_DISCRETE);
  return input.channels();
}

// static
bool UnionTraits<media::mojom::ChannelLayoutConfigDataView,
                 media::ChannelLayoutConfig>::
    Read(media::mojom::ChannelLayoutConfigDataView input,
         media::ChannelLayoutConfig* output) {
  switch (input.tag()) {
    case media::mojom::ChannelLayoutConfigDataView::Tag::kPredefinedLayout: {
      media::mojom::ChannelLayout predefined_layout;
      if (!input.ReadPredefinedLayout(&predefined_layout)) {
        return false;
      }

      if (predefined_layout == media::mojom::ChannelLayout::kDiscrete) {
        return false;
      }

      *output = media::ChannelLayoutConfig::FromLayout(
          EnumTraits<media::mojom::ChannelLayout,
                     media::ChannelLayout>::FromMojom(predefined_layout));
      return true;
    }
    case media::mojom::ChannelLayoutConfigDataView::Tag::kDiscreteChannels: {
      uint32_t channels = input.discrete_channels();
      if (channels == 0) {
        return false;
      }
      *output =
          media::ChannelLayoutConfig(media::CHANNEL_LAYOUT_DISCRETE, channels);
      return true;
    }
  }
  NOTREACHED();
}
}  // namespace mojo
