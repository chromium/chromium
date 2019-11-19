// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_

namespace extensions {

// Contains constants corresponding to keys in _behavior_features.json.
// One day we might want to auto generate these.
namespace behavior_feature {

extern const char kDoNotSync[];
extern const char kZoomWithoutBubble[];
extern const char kAllowUsbDevicesPermissionInterfaceClass[];
extern const char kSigninScreen[];
extern const char kAllowDeprecatedAudioApi[];
extern const char kAllowSecondaryKioskAppEnabledOnLaunch[];

}  // namespace behavior_feature

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_
