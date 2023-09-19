// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_SCREEN_CAPTURE_KIT_SWIZZLER_H_
#define MEDIA_AUDIO_MAC_SCREEN_CAPTURE_KIT_SWIZZLER_H_

#include <memory>

#include "base/apple/scoped_objc_class_swizzler.h"

namespace media {
std::unique_ptr<base::apple::ScopedObjCClassSwizzler> SwizzleScreenCaptureKit();
}

#endif  // MEDIA_AUDIO_MAC_SCREEN_CAPTURE_KIT_SWIZZLER_H_
