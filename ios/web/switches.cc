// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/switches.h"

namespace web {
namespace switches {

// NOTE: The gn arg `ios_enable_javascript_flags` must be set to `true` in order
// for these flags to have any effect.

// Prevents the injection of all Javascript injected through JavaScriptFeatures.
extern const char kDisableAllInjectedScripts[] = "disable-all-injected-scripts";
// Prevents most injection of Javascript injected through JavaScriptFeatures,
// however basic shared scripts which setup WebFrames are still injected.
extern const char kDisableInjectedFeatureScripts[] =
    "disable-injected-feature-scripts";
// Prevents the listed scripts from being injected. The value must be a comma
// separated string of `injection_token_`s from the JavaScriptFeatures to be
// disabled.
// For example, to disable context menu JS, use:
// `--disable-listed-scripts=all_frames_context_menu,main_frame_context_menu`
extern const char kDisableListedScripts[] = "disable-listed-scripts";
// Enables only the listed scripts. The value must be a comma separated string
// of `injection_token_`s from the JavaScriptFeatures to be enabled.
// For example, to only enable context menu JS, use:
// `--enable-listed-scripts=gcrweb,common,message,all_frames_context_menu,
//     main_frame_context_menu`
// Note that all dependencies, must be manually enabled when using this flag.
extern const char kEnableListedScripts[] = "enable-listed-scripts";

}  // namespace switches
}  // namespace web
