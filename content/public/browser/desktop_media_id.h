// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_

#include <string>
#include <tuple>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Type used to identify desktop media sources. It's converted to string and
// stored in MediaStreamRequest::requested_video_device_id.
struct CONTENT_EXPORT DesktopMediaID {
 public:
  enum Type { TYPE_NONE, TYPE_SCREEN, TYPE_WINDOW, TYPE_WEB_CONTENTS };

  typedef intptr_t Id;

  // Represents an "unset" value for either |id| or |window_id|.
  static const Id kNullId;
  // Represents a fake id to create a dummy capturer for autotests.
  static const Id kFakeId;

#if defined(USE_AURA) || defined(OS_MACOSX)
  // Assigns integer identifier to the |window| and returns its DesktopMediaID.
  static DesktopMediaID RegisterNativeWindow(Type type,
                                             gfx::NativeWindow window);

  // Returns the Window that was previously registered using
  // RegisterNativeWindow(), else nullptr.
  static gfx::NativeWindow GetNativeWindowById(const DesktopMediaID& id);
#endif  // USE_AURA || OS_MACOSX

  constexpr DesktopMediaID() = default;

  constexpr DesktopMediaID(Type type, Id id) : type(type), id(id) {}

  constexpr DesktopMediaID(Type type,
                           Id id,
                           WebContentsMediaCaptureId web_contents_id)
      : type(type), id(id), web_contents_id(web_contents_id) {}

  constexpr DesktopMediaID(Type type, Id id, bool audio_share)
      : type(type), id(id), audio_share(audio_share) {}

  // Operators so that DesktopMediaID can be used with STL containers.
  bool operator<(const DesktopMediaID& other) const;
  bool operator==(const DesktopMediaID& other) const;

  bool is_null() const { return type == TYPE_NONE; }
  std::string ToString() const;
  static DesktopMediaID Parse(const std::string& str);

  Type type = TYPE_NONE;

  // The IDs referring to the target native screen or window.  |id| will be
  // non-null if and only if it refers to a native screen/window.  |window_id|
  // will be non-null if and only if it refers to an Aura window.  Note that is
  // it possible for both of these to be non-null, which means both IDs are
  // referring to the same logical window.
  Id id = kNullId;
  // TODO(miu): Make this an int, after clean-up for http://crbug.com/513490.
  Id window_id = kNullId;

  // This records whether the desktop share has sound or not.
  bool audio_share = false;

  // This id contains information for WebContents capture.
  WebContentsMediaCaptureId web_contents_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
