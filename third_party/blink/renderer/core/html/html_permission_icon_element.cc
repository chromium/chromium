// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_icon_element.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

using mojom::blink::PermissionName;

HTMLPermissionIconElement::HTMLPermissionIconElement(Document& document)
    : HTMLSpanElement(document) {
  SetIdAttribute(shadow_element_names::kIdPermissionIcon);
  SetShadowPseudoId(shadow_element_names::kIdPermissionIcon);
}

void HTMLPermissionIconElement::SetIcon(PermissionName permission_type,
                                        bool is_precise_location) {
  if (is_icon_set_) {
    return;
  }
  switch (permission_type) {
    case PermissionName::GEOLOCATION:
      setInnerHTML(UncompressResourceAsASCIIString(
          is_precise_location ? IDR_PERMISSION_ICON_LOCATION_PRECISE_SVG
                              : IDR_PERMISSION_ICON_LOCATION_SVG));
      break;
    case PermissionName::VIDEO_CAPTURE:
      setInnerHTML(String(
          UncompressResourceAsASCIIString(IDR_PERMISSION_ICON_CAMERA_SVG)));
      break;
    case PermissionName::AUDIO_CAPTURE:
      setInnerHTML(
          UncompressResourceAsASCIIString(IDR_PERMISSION_ICON_MICROPHONE_SVG));
      break;
    default:
      return;
  }
  is_icon_set_ = true;
}
}  // namespace blink
