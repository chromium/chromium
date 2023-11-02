// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_rendering_mode.h"

#include "base/strings/string_number_conversions.h"

#include <string>

namespace media {

std::ostream& operator<<(std::ostream& os,
                         const MediaFoundationRenderingMode& render_mode) {
  std::string mode;
  switch (render_mode) {
    case (MediaFoundationRenderingMode::FrameServer):
      mode = "Frame Server";
      break;
    case (MediaFoundationRenderingMode::DirectComposition):
      mode = "Direct Composition";
      break;
    default:
      mode = "UNEXPECTED RENDERING MODE " +
             base::NumberToString(static_cast<int>(render_mode));
  }

  return os << mode;
}

}  // namespace media
