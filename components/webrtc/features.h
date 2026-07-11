// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_FEATURES_H_
#define COMPONENTS_WEBRTC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace webrtc::features {

COMPONENT_EXPORT(COMPONENTS_WEBRTC_FEATURES)
BASE_DECLARE_FEATURE(kWebRTCBoostMediaIOThreads);

}  // namespace webrtc::features

#endif  // COMPONENTS_WEBRTC_FEATURES_H_
