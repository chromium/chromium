// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CONSTANTS_H_
#define REMOTING_BASE_CONSTANTS_H_

#include <array>
#include <string_view>

#include "build/build_config.h"

namespace remoting {

// Namespace used for chromoting XMPP stanzas.
extern const char kChromotingXmlNamespace[];

// Channel names.
extern const char kAudioChannelName[];
extern const char kControlChannelName[];
extern const char kEventChannelName[];
extern const char kVideoChannelName[];
extern const char kVideoStatsChannelNamePrefix[];

// MIME types for the clipboard.
extern const char kMimeTypeTextUtf8[];

const int kDefaultDpi = 96;

// The video frame rate.
constexpr int kTargetFrameRate = 30;

#if BUILDFLAG(IS_LINUX)
inline constexpr char kChromeRemoteDesktopSessionEnvVar[] =
    "CHROME_REMOTE_DESKTOP_SESSION";

// This list was created by looking at the MIME types claimed by some Wayland
// and XWayland apps that put text onto the clipboard. It is ordered by
// priority, preferring modern explicit UTF-8 formats over legacy string
// formats.
inline constexpr std::array<std::string_view, 5> kTextMimeTypes = {
    "text/plain;charset=utf-8", "UTF8_STRING", "text/plain", "STRING", "TEXT"};
#endif

}  // namespace remoting

#endif  // REMOTING_BASE_CONSTANTS_H_
