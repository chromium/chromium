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
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

namespace content {

// This class encapsulates a cross-platform description of a cursor.  Platform
// specific methods are provided to translate the cross-platform cursor into a
// platform specific cursor.  It is also possible to serialize / de-serialize a
// WebCursor. This class is highly similar to ui::Cursor.
class CONTENT_EXPORT WebCursor {
 public:
  WebCursor();
  explicit WebCursor(const ui::Cursor& info);
  explicit WebCursor(const WebCursor& other);
  ~WebCursor();

  const ui::Cursor& cursor() const { return cursor_; }

  // Sets the ui::Cursor |cursor|; returns whether it has reasonable values.
  bool SetCursor(const ui::Cursor& cursor);

  // Equality operator; performs bitmap content comparison as needed.
  bool operator==(const WebCursor& other) const;
  bool operator!=(const WebCursor& other) const;

  // Returns a native cursor representing the current WebCursor instance.
  gfx::NativeCursor GetNativeCursor();

#if defined(USE_AURA)
  // Updates |device_scale_factor_| and |rotation_| based on |display|.
  void SetDisplayInfo(const display::Display& display);

  void CreateScaledBitmapAndHotspotFromCustomData(SkBitmap* bitmap,
                                                  gfx::Point* hotspot,
                                                  float* scale);

  bool has_custom_cursor_for_test() const { return !!custom_cursor_; }
#endif

 private:
  // Platform specific cleanup.
  void CleanupPlatformData();

  float GetCursorScaleFactor(SkBitmap* bitmap);

  // The basic cursor info.
  ui::Cursor cursor_;

#if defined(USE_AURA) || BUILDFLAG(IS_OZONE)
  // Only used for custom cursors.
  float device_scale_factor_ = 1.f;
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;
#endif

#if BUILDFLAG(IS_OZONE)
  // This matches ozone drm_util.cc's kDefaultCursorWidth/Height.
  static constexpr int kDefaultMaxSize = 64;
  gfx::Size maximum_cursor_size_ = {kDefaultMaxSize, kDefaultMaxSize};
#endif

#if defined(USE_AURA)
  absl::optional<ui::Cursor> custom_cursor_;
#endif
};

}  // namespace content

#endif  // CONTENT_COMMON_CURSORS_WEBCURSOR_H_
