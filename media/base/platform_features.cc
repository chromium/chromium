// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/platform_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace media {

bool IsVp9kSVCHWDecodingEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  // arm: Support is API and driver dependent:
  // - V4L2 state*less* API decoder is not capable of decoding VP9 kSVC stream.
  // - V4L2 state*ful* API decoder is capable of decoding, but is driver
  // dependent. x86: Always supported.
  return true;
#elif BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40286220): Experiment to enable on Windows.
  return base::FeatureList::IsEnabled(media::kD3D11Vp9kSVCHWDecoding);
#else
  return false;
#endif
}

bool IsAV1SpatialLayerHWDecodingEnabled() {
#if BUILDFLAG(IS_MAC)
  // This only restricts VideoToolbox's AV1 hardware decoder where one exists;
  // Macs without AV1 hardware decode (before M3) never use VideoToolbox for
  // AV1, spatial layers or not. VideoToolbox decodes AV1 spatial layers; other
  // hardware decoders are driven per frame and are not known to. VideoToolbox
  // was reported once to corrupt spatial-layer prediction, on an unrecorded
  // macOS version. macOS 26 is the oldest version verified bit-exact against
  // libdav1d, so older versions stay on the per-frame path
  // (crbug.com/573939397).
  return base::mac::MacOSMajorVersion() >= 26 &&
         base::FeatureList::IsEnabled(kVideoToolboxAV1SpatialLayerDecoding);
#else
  return false;
#endif
}

}  // namespace media
