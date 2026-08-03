// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_OPTIONS_CONSTANTS_H_
#define REMOTING_BASE_SESSION_OPTIONS_CONSTANTS_H_

#include "build/build_config.h"

namespace remoting {

// Session option key names.
extern const char kSessionOptionDetectUpdatedRegion[];
extern const char kSessionOptionCaptureVideoOnDedicatedThread[];
#if BUILDFLAG(IS_MAC)
extern const char kSessionOptionEnableSckCapturer[];
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
extern const char kSessionOptionAllowDxgiCapturer[];
#endif  // BUILDFLAG(IS_WIN)
extern const char kSessionOptionDisableUdp[];
extern const char kSessionOptionVp9EncoderSpeed[];
extern const char kSessionOptionAv1ActiveMap[];
extern const char kSessionOptionAv1EncoderSpeed[];

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_OPTIONS_CONSTANTS_H_
