// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SWITCHES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SWITCHES_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace switches {

// base::Feature should be use instead of switches where possible.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
BLINK_COMMON_EXPORT extern const char kAllowPreCommitInput[];
BLINK_COMMON_EXPORT extern const char kBlinkSettings[];
BLINK_COMMON_EXPORT extern const char kDefaultTileWidth[];
BLINK_COMMON_EXPORT extern const char kDefaultTileHeight[];
BLINK_COMMON_EXPORT extern const char kDisableImageAnimationResync[];
BLINK_COMMON_EXPORT extern const char kDisableLowResTiling[];
BLINK_COMMON_EXPORT extern const char kDisablePartialRaster[];
BLINK_COMMON_EXPORT extern const char kDisablePreferCompositingToLCDText[];
BLINK_COMMON_EXPORT extern const char kDisableRGBA4444Textures[];
BLINK_COMMON_EXPORT extern const char kDisableThreadedScrolling[];
BLINK_COMMON_EXPORT extern const char kDisableZeroCopy[];
BLINK_COMMON_EXPORT extern const char
    kEnableGpuMemoryBufferCompositorResources[];
BLINK_COMMON_EXPORT extern const char kEnableLowResTiling[];
BLINK_COMMON_EXPORT extern const char kEnablePreferCompositingToLCDText[];
BLINK_COMMON_EXPORT extern const char kEnableRGBA4444Textures[];
BLINK_COMMON_EXPORT extern const char kEnableZeroCopy[];
BLINK_COMMON_EXPORT extern const char kForceLegacyDefaultReferrerPolicy[];
BLINK_COMMON_EXPORT extern const char kGpuRasterizationMSAASampleCount[];
BLINK_COMMON_EXPORT extern const char kIntensiveWakeUpThrottlingPolicy[];
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottlingPolicy_ForceDisable[];
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottlingPolicy_ForceEnable[];
BLINK_COMMON_EXPORT extern const char kMaxUntiledLayerHeight[];
BLINK_COMMON_EXPORT extern const char kMaxUntiledLayerWidth[];
BLINK_COMMON_EXPORT extern const char kMinHeightForGpuRasterTile[];
BLINK_COMMON_EXPORT extern const char kNetworkQuietTimeout[];
BLINK_COMMON_EXPORT extern const char kPassiveListenersDefault[];
BLINK_COMMON_EXPORT extern const char kShowLayoutShiftRegions[];
BLINK_COMMON_EXPORT extern const char kShowPaintRects[];
BLINK_COMMON_EXPORT extern const char kTouchTextSelectionStrategy[];

BLINK_COMMON_EXPORT extern const char kUserAgentClientHintDisable[];
}  // namespace switches
}  // namespace blink

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
