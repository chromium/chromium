// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_VIEW_SOURCE_RENDERING_PREFERENCES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_VIEW_SOURCE_RENDERING_PREFERENCES_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// A static class that stores a per-renderer process line wrapping preference
// for view source pages, as sent by the browser.
class BLINK_COMMON_EXPORT ViewSourceLineWrappingPreference final {
 public:
  static void Set(bool value);
  static bool Get();
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_VIEW_SOURCE_RENDERING_PREFERENCES_H_
