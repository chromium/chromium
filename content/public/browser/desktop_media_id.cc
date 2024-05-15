// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_media_id.h"

#include <stdint.h>

#include <vector>

#include "base/containers/id_map.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/media/desktop_media_window_registry.h"

namespace content {

const char kScreenPrefix[] = "screen";
const char kWindowPrefix[] = "window";

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
// static
DesktopMediaID DesktopMediaID::RegisterNativeWindow(DesktopMediaID::Type type,
                                                    gfx::NativeWindow window) {
  DCHECK(type == TYPE_SCREEN || type == TYPE_WINDOW);
  DCHECK(window);
  DesktopMediaID media_id(type, kNullId);
  media_id.window_id =
      DesktopMediaWindowRegistry::GetInstance()->RegisterWindow(window);
  return media_id;
}

// static
gfx::NativeWindow DesktopMediaID::GetNativeWindowById(
    const DesktopMediaID& id) {
  return DesktopMediaWindowRegistry::GetInstance()->GetWindowById(id.window_id);
}
#endif

bool DesktopMediaID::operator<(const DesktopMediaID& other) const {
  return std::tie(type, id, window_id, web_contents_id, audio_share) <
         std::tie(other.type, other.id, other.window_id, other.web_contents_id,
                  other.audio_share);
}

bool DesktopMediaID::operator==(const DesktopMediaID& other) const {
  return type == other.type && id == other.id && window_id == other.window_id &&
         web_contents_id == other.web_contents_id &&
         audio_share == other.audio_share;
}

bool DesktopMediaID::operator!=(const DesktopMediaID& other) const {
  return !(*this == other);
}

// static
// Input string should in format:
// - For WebContents:
//   web-contents-media-stream://"render_process_id":"main_render_frame_id",
//   with optional local_echo=false specified as a "query string".
// - For screen: screen:window_id:native_window_id
// - For window: window:window_id:native_window_id
DesktopMediaID DesktopMediaID::Parse(const std::string& str) {
  // For WebContents type.
  WebContentsMediaCaptureId web_id;
  if (WebContentsMediaCaptureId::Parse(str, &web_id))
    return DesktopMediaID(TYPE_WEB_CONTENTS, 0, web_id);

  // For screen and window types.
  std::vector<std::string> parts = base::SplitString(
      str, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (parts.size() != 3)
    return DesktopMediaID();

  Type type = TYPE_NONE;
  if (parts[0] == kScreenPrefix) {
    type = TYPE_SCREEN;
  } else if (parts[0] == kWindowPrefix) {
    type = TYPE_WINDOW;
  } else {
    return DesktopMediaID();
  }

  int64_t id;
  if (!base::StringToInt64(parts[1], &id))
    return DesktopMediaID();

  DesktopMediaID media_id(type, id);

  int64_t window_id;
  if (!base::StringToInt64(parts[2], &window_id))
    return DesktopMediaID();
  media_id.window_id = window_id;

  return media_id;
}

std::string DesktopMediaID::ToString() const {
  std::string prefix;
  switch (type) {
    case TYPE_NONE:
      NOTREACHED_IN_MIGRATION();
      return std::string();
    case TYPE_SCREEN:
      prefix = kScreenPrefix;
      break;
    case TYPE_WINDOW:
      prefix = kWindowPrefix;
      break;
    case TYPE_WEB_CONTENTS:
      return web_contents_id.ToString();
  }
  DCHECK(!prefix.empty());

  // Screen and Window types.
  prefix.append(":");
  prefix.append(base::NumberToString(id));

  prefix.append(":");
  prefix.append(base::NumberToString(window_id));

  return prefix;
}

}  // namespace content
