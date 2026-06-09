// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IMMERSIVE_PLAYBACK_OPTIONS_H_
#define CONTENT_PUBLIC_BROWSER_IMMERSIVE_PLAYBACK_OPTIONS_H_

#include <optional>

namespace content {

// Represents the stereo mode for immersive Picture-in-Picture playback.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ImmersiveStereoMode {
  kMono = 0,
  kMultiviewLeftPrimary = 1,
  kMultiviewRightPrimary = 2,
  kSideBySide = 3,
  kTopBottom = 4,
};

// Represents the projection type for immersive Picture-in-Picture playback.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ImmersiveProjectionType {
  kQuad = 0,
  kSphere = 1,
  kHemisphere = 2,
  kCustom = 3,
};

// Represents the options for an immersive Picture-in-Picture session.
struct ImmersiveOptions {
  ImmersiveStereoMode stereo_mode;
  ImmersiveProjectionType projection_type;
};

// Represents the result status of an immersive Picture-in-Picture playback
// confirmation request.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ImmersivePlaybackConfirmationStatus {
  kConfirmed = 0,
  kDeclined = 1,
  kCanceled = 2,
  kFailed = 3,
};

// Represents the result of an immersive Picture-in-Picture playback
// confirmation.
struct ImmersivePlaybackConfirmationResult {
  ImmersivePlaybackConfirmationStatus status;
  std::optional<ImmersiveOptions> options;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IMMERSIVE_PLAYBACK_OPTIONS_H_
