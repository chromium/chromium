// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace content {

#if BUILDFLAG(IS_OZONE)
void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (rotation_ == display.panel_rotation() &&
      device_scale_factor_ == display.device_scale_factor() &&
      maximum_cursor_size_ == display.maximum_cursor_size())
    return;
  device_scale_factor_ = display.device_scale_factor();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The cursor should use the panel's physical rotation instead of
  // rotation. They can be different on ChromeOS but the same on
  // other platforms.
  rotation_ = display.panel_rotation();
#endif
  maximum_cursor_size_ = display.maximum_cursor_size();
  // TODO(oshima): Identify if it's possible to remove this check here and move
  // the kDefaultMaxSize constants to a single place. crbug.com/603512
  if (maximum_cursor_size_.width() == 0 || maximum_cursor_size_.height() == 0)
    maximum_cursor_size_ = gfx::Size(kDefaultMaxSize, kDefaultMaxSize);
  custom_cursor_.reset();
}

float WebCursor::GetCursorScaleFactor(SkBitmap* bitmap) {
  DCHECK_LT(0, maximum_cursor_size_.width());
  DCHECK_LT(0, maximum_cursor_size_.height());
  return std::min(
      {device_scale_factor_ / cursor_.image_scale_factor(),
       static_cast<float>(maximum_cursor_size_.width()) / bitmap->width(),
       static_cast<float>(maximum_cursor_size_.height()) / bitmap->height()});
}
#endif

}  // namespace content
