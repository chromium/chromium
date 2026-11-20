// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_DIALOG_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_DIALOG_UTILS_H_

namespace web {
class WebState;
}  // namespace web

// Specific dialogs, prompts, and permission requests handled while an
// `ActorTask` is actuating a tab.
// Logged to the `IOS.Actor.Dialog.Suppressed` histogram.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSDialogDuringActuation)
enum class IOSDialogDuringActuation {
  kCameraPermission = 0,
  kMicrophonePermission = 1,
  kCameraAndMicrophonePermission = 2,
  kGeolocationPermission = 3,
  kMaxValue = kGeolocationPermission,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSDialogDuringActuation)

// Histogram name for dialogs and prompts suppressed during actuation.
inline constexpr char kActorDialogSuppressedHistogram[] =
    "IOS.Actor.Dialog.Suppressed";

// Categories of prompts, dialogs, and disruptive requests suppressed while an
// `ActorTask` is actuating a tab. Each category is backed by its own parameter
// under `kActorSuppressDialogs`.
enum class ActorSuppressedDialogType {
  // Permission-gated web API requests.
  kPermissions,
};

// Returns whether dialogs of `type` originating from `web_state` should be
// suppressed without prompting the user while an `ActorTask` is actuating the
// tab. Returns false if `web_state` is null.
bool ShouldSuppressDialogDuringActuation(web::WebState* web_state,
                                         ActorSuppressedDialogType type);

// Records that `dialog` was suppressed because the tab was being actuated.
void RecordDialogSuppressedDuringActuation(IOSDialogDuringActuation dialog);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_DIALOG_UTILS_H_
