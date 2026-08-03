// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_OPTIONS_H_
#define REMOTING_BASE_SESSION_OPTIONS_H_

#include <optional>
#include <ostream>

#include "build/build_config.h"

namespace base {
class DictValue;
}  // namespace base

namespace remoting {

// Session based host options sending from client. This struct stores session
// configuration from client side to control the behavior of other host
// components.
struct SessionOptions {
  SessionOptions();
  ~SessionOptions();

  SessionOptions(const SessionOptions& other);
  SessionOptions& operator=(const SessionOptions& other);
  SessionOptions(SessionOptions&& other);
  SessionOptions& operator=(SessionOptions&& other);

  bool operator==(const SessionOptions& other) const;

  // Parses key-value pairs from `dict` into a `SessionOptions` instance.
  // Unsupported keys or values that cannot be converted to the expected type
  // are ignored with a warning log.
  static SessionOptions Parse(const base::DictValue& dict);

  // Whether to detect updated regions when capturing the screen.
  // Corresponding option key: Detect-Updated-Region
  std::optional<bool> detect_updated_region;

  // Whether video capture should run on a dedicated thread.
  // Corresponding option key: Capture-Video-On-Dedicated-Thread
  std::optional<bool> capture_video_on_dedicated_thread;

#if BUILDFLAG(IS_MAC)
  // Whether to enable ScreenCaptureKit capturer on macOS.
  // Corresponding option key: Enable-Sck-Capturer
  std::optional<bool> enable_sck_capturer;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  // Whether to allow DXGI capturer on Windows.
  // Corresponding option key: Allow-Dxgi-Capturer
  std::optional<bool> allow_dxgi_capturer;
#endif  // BUILDFLAG(IS_WIN)

  // Whether to disable UDP connections.
  // Corresponding option key: Disable-UDP
  std::optional<bool> disable_udp;

  // Encoder speed for VP9 video codec.
  // Corresponding option key: Vp9-Encoder-Speed
  std::optional<int> vp9_encoder_speed;

  // Whether active map is enabled for AV1 video codec.
  // Corresponding option key: Av1-Active-Map
  std::optional<bool> av1_active_map;

  // Encoder speed for AV1 video codec.
  // Corresponding option key: Av1-Encoder-Speed
  std::optional<int> av1_encoder_speed;
};

std::ostream& operator<<(std::ostream& os,
                         const SessionOptions& session_options);

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_OPTIONS_H_
