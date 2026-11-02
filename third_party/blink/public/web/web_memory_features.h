// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_FEATURES_H_

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class BLINK_EXPORT WebMemoryFeatures {
 public:
  WebMemoryFeatures() = delete;

#if BUILDFLAG(IS_ANDROID)
  // Notifies memory-related features whether the renderer process has any
  // visible clients.
  static void SetRendererVisible(bool visible);
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_FEATURES_H_
