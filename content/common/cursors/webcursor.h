// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CURSORS_WEBCURSOR_H_
#define CONTENT_COMMON_CURSORS_WEBCURSOR_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_AURA)
#include <optional>
#endif

namespace content {

// NOTE(https://crbug.com/1149906): This class is deprecated and ui::Cursor
// should be used instead.
//
// This class encapsulates a cross-platform description of a cursor.  Platform
// specific methods are provided to translate the cross-platform cursor into a
// platform specific cursor.  It is also possible to serialize / de-serialize a
// WebCursor. This class is highly similar to ui::Cursor.
class CONTENT_EXPORT WebCursor {
 public:
  WebCursor();
  explicit WebCursor(const ui::Cursor& info);
  ~WebCursor();

  const ui::Cursor& cursor() const { return cursor_; }

  // Returns a native cursor representing the current WebCursor instance.
  gfx::NativeCursor GetNativeCursor();

#if defined(USE_AURA)
  // Updates |device_scale_factor_| and |rotation_| based on |window|'s
  // preferred scale (if any) and its display information.
  void UpdateDisplayInfoForWindow(aura::Window* window);

  bool has_custom_cursor_for_test() const { return !!custom_cursor_; }
#endif

 private:
  float GetCursorScaleFactor(SkBitmap* bitmap);

  // The basic cursor info.
  ui::Cursor cursor_;

#if defined(USE_AURA)
  // Only used for custom cursors.
  float device_scale_factor_ = 1.f;
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This matches ozone drm_util.cc's kDefaultCursorWidth/Height.
  static constexpr int kDefaultMaxSize = 64;
  gfx::Size maximum_cursor_size_ = {kDefaultMaxSize, kDefaultMaxSize};
#endif

#if defined(USE_AURA)
  std::optional<ui::Cursor> custom_cursor_;
#endif
};

}  // namespace content

#endif  // CONTENT_COMMON_CURSORS_WEBCURSOR_H_
