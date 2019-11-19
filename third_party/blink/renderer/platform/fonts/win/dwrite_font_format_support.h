// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_DWRITE_FONT_FORMAT_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_DWRITE_FONT_FORMAT_SUPPORT_H_

namespace blink {

// Return whether DirectWrite on this system supports variable fonts for
// retrieving metrics and performing rasterization.
bool DWriteVersionSupportsVariations();
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_DWRITE_FONT_FORMAT_SUPPORT_H_
