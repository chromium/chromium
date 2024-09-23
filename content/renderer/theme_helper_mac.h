// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_THEME_HELPER_MAC_H_
#define CONTENT_RENDERER_THEME_HELPER_MAC_H_

namespace content {

// MacOS 10.14 (Mojave) disabled subpixel anti-aliasing by default, but this can
// be overridden with a setting (CGFontRenderingFontSmoothingDisabled).
bool IsSubpixelAntialiasingAvailable();

}  // namespace content

#endif  // CONTENT_RENDERER_THEME_HELPER_MAC_H_
