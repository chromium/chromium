// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/frame_owner_properties.h"

#include <algorithm>
#include <iterator>

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace content {

FrameOwnerProperties ConvertWebFrameOwnerPropertiesToFrameOwnerProperties(
    const blink::WebFrameOwnerProperties& web_frame_owner_properties) {
  FrameOwnerProperties result;

  result.name = web_frame_owner_properties.name.Utf8();
  result.scrolling_mode = web_frame_owner_properties.scrolling_mode;
  result.margin_width = web_frame_owner_properties.margin_width;
  result.margin_height = web_frame_owner_properties.margin_height;
  result.allow_fullscreen = web_frame_owner_properties.allow_fullscreen;
  result.allow_payment_request =
      web_frame_owner_properties.allow_payment_request;
  result.is_display_none = web_frame_owner_properties.is_display_none;
  result.required_csp = web_frame_owner_properties.required_csp.Utf8();

  return result;
}

blink::WebFrameOwnerProperties
ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(
    const FrameOwnerProperties& frame_owner_properties) {
  blink::WebFrameOwnerProperties result;

  result.name = blink::WebString::FromUTF8(frame_owner_properties.name);
  result.scrolling_mode = frame_owner_properties.scrolling_mode;
  result.margin_width = frame_owner_properties.margin_width;
  result.margin_height = frame_owner_properties.margin_height;
  result.allow_fullscreen = frame_owner_properties.allow_fullscreen;
  result.allow_payment_request = frame_owner_properties.allow_payment_request;
  result.is_display_none = frame_owner_properties.is_display_none;
  result.required_csp =
      blink::WebString::FromUTF8(frame_owner_properties.required_csp);

  return result;
}

}  // namespace content
