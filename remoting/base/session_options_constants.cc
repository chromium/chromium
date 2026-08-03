// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_options_constants.h"

#include "build/build_config.h"

namespace remoting {

const char kSessionOptionDetectUpdatedRegion[] = "Detect-Updated-Region";
const char kSessionOptionCaptureVideoOnDedicatedThread[] =
    "Capture-Video-On-Dedicated-Thread";
#if BUILDFLAG(IS_MAC)
const char kSessionOptionEnableSckCapturer[] = "Enable-Sck-Capturer";
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
const char kSessionOptionAllowDxgiCapturer[] = "Allow-Dxgi-Capturer";
#endif  // BUILDFLAG(IS_WIN)
const char kSessionOptionDisableUdp[] = "Disable-UDP";
const char kSessionOptionVp9EncoderSpeed[] = "Vp9-Encoder-Speed";
const char kSessionOptionAv1ActiveMap[] = "Av1-Active-Map";
const char kSessionOptionAv1EncoderSpeed[] = "Av1-Encoder-Speed";

}  // namespace remoting
