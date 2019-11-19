// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"

namespace content {

WebCursor::~WebCursor() {
  CleanupPlatformData();
}

WebCursor::WebCursor(const CursorInfo& info) {
  SetInfo(info);
}

WebCursor::WebCursor(const WebCursor& other) {
  CopyAllData(other);
}

WebCursor& WebCursor::operator=(const WebCursor& other) {
  CleanupPlatformData();
  CopyAllData(other);
  return *this;
}

bool WebCursor::SetInfo(const CursorInfo& info) {
  static constexpr int kMaxSize = 1024;
  if (info.image_scale_factor < 0.01f || info.image_scale_factor > 100.f ||
      info.custom_image.width() > kMaxSize ||
      info.custom_image.height() > kMaxSize ||
      info.custom_image.width() / info.image_scale_factor > kMaxSize ||
      info.custom_image.height() / info.image_scale_factor > kMaxSize) {
    return false;
  }

  CleanupPlatformData();
  info_ = info;

  // Clamp the hotspot to the custom image's dimensions.
  if (info_.type == ui::CursorType::kCustom) {
    info_.hotspot.set_x(std::max(
        0, std::min(info_.custom_image.width() - 1, info_.hotspot.x())));
    info_.hotspot.set_y(std::max(
        0, std::min(info_.custom_image.height() - 1, info_.hotspot.y())));
  }

  return true;
}

bool WebCursor::operator==(const WebCursor& other) const {
  return info_ == other.info_ &&
#if defined(USE_AURA) || defined(USE_OZONE)
         rotation_ == other.rotation_ &&
#endif
         IsPlatformDataEqual(other);
}

bool WebCursor::operator!=(const WebCursor& other) const {
  return !(*this == other);
}

void WebCursor::CopyAllData(const WebCursor& other) {
  SetInfo(other.info_);
  CopyPlatformData(other);
}

}  // namespace content
