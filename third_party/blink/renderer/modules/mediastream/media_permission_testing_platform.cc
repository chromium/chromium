// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_permission_testing_platform.h"

#include <memory>

#include "media/base/media_permission.h"

namespace blink {

MediaPermissionTestingPlatform::MediaPermissionTestingPlatform(
    std::unique_ptr<media::MediaPermission> media_permission)
    : media_permission_(std::move(media_permission)) {}

media::MediaPermission*
MediaPermissionTestingPlatform::GetWebRTCMediaPermission(WebLocalFrame*) {
  return media_permission_.get();
}

}  // namespace blink
