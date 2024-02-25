// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_METADATA_H_
#define REMOTING_HOST_INPUT_INJECTOR_METADATA_H_

#if defined(WEBRTC_USE_PIPEWIRE)
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"
#endif  // defined(WEBRTC_USE_PIPEWIRE)

namespace remoting {

struct InputInjectorMetadata {
#if defined(WEBRTC_USE_PIPEWIRE)
  // Details of the XDG desktop portal session (required by the wayalnd input
  // injector).
  webrtc::xdg_portal::SessionDetails session_details;
#endif  // defined(WEBRTC_USE_PIPEWIRE)
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_METADATA_H_
