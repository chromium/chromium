// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/frame_owner_properties_converter.h"

namespace mojo {

blink::WebFrameOwnerProperties
TypeConverter<blink::WebFrameOwnerProperties,
              blink::mojom::FrameOwnerProperties>::
    Convert(const blink::mojom::FrameOwnerProperties& mojo_properties) {
  blink::WebFrameOwnerProperties result;
  result.name = blink::WebString::FromUTF8(mojo_properties.name);
  result.scrollbar_mode = mojo_properties.scrollbar_mode;
  result.margin_width = mojo_properties.margin_width;
  result.margin_height = mojo_properties.margin_height;
  result.allow_fullscreen = mojo_properties.allow_fullscreen;
  result.allow_payment_request = mojo_properties.allow_payment_request;
  result.is_display_none = mojo_properties.is_display_none;
  result.color_scheme = mojo_properties.color_scheme;

  return result;
}

blink::mojom::FrameOwnerPropertiesPtr
TypeConverter<blink::mojom::FrameOwnerPropertiesPtr,
              blink::WebFrameOwnerProperties>::
    Convert(const blink::WebFrameOwnerProperties& web_frame_owner_properties) {
  blink::mojom::FrameOwnerPropertiesPtr mojo_properties =
      blink::mojom::FrameOwnerProperties::New();

  mojo_properties->name = web_frame_owner_properties.name.Utf8();
  mojo_properties->scrollbar_mode = web_frame_owner_properties.scrollbar_mode;
  mojo_properties->margin_width = web_frame_owner_properties.margin_width;
  mojo_properties->margin_height = web_frame_owner_properties.margin_height;
  mojo_properties->allow_fullscreen =
      web_frame_owner_properties.allow_fullscreen;
  mojo_properties->allow_payment_request =
      web_frame_owner_properties.allow_payment_request;
  mojo_properties->is_display_none = web_frame_owner_properties.is_display_none;
  mojo_properties->color_scheme = web_frame_owner_properties.color_scheme;

  return mojo_properties;
}

}  // namespace mojo
