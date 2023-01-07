// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FRAME_OWNER_PROPERTIES_CONVERTER_H_
#define CONTENT_RENDERER_FRAME_OWNER_PROPERTIES_CONVERTER_H_

#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"

namespace mojo {

// These conversions are necessary because we can't expose
// blink::mojom::blink::FrameOwnerProperties on the external API,
// e.g. WebRemoteFrame::CreateLocalChild, and that all
// content messages need to be removed first. Once that is done then
// WebFrameOwnerProperties will be removed and
// blink::mojom::blink::FrameOwnerProperties can be used directly inside blink.

template <>
struct TypeConverter<blink::WebFrameOwnerProperties,
                     blink::mojom::FrameOwnerProperties> {
  static blink::WebFrameOwnerProperties Convert(
      const blink::mojom::FrameOwnerProperties& mojo_properties);
};

template <>
struct TypeConverter<blink::mojom::FrameOwnerPropertiesPtr,
                     blink::WebFrameOwnerProperties> {
  static blink::mojom::FrameOwnerPropertiesPtr Convert(
      const blink::WebFrameOwnerProperties& properties);
};

}  // namespace mojo

#endif  // CONTENT_RENDERER_FRAME_OWNER_PROPERTIES_CONVERTER_H_
