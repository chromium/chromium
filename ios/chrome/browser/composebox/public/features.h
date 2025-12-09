// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

// Used to enable development tools for the composebox.
BASE_DECLARE_FEATURE(kComposeboxDevTools);

// Parameter of `kComposeboxDevTools` to delay image loading.
extern const char kImageLoadDelayMsParam[];
// Parameter of `kComposeboxDevTools` to delay image upload.
extern const char kUploadDelayMsParam[];
// Parameter of `kComposeboxDevTools` to force image upload failure.
extern const char kForceUploadFailureParam[];

// Returns the configured image load delay.
base::TimeDelta GetImageLoadDelay();

// Returns the configured upload delay.
base::TimeDelta GetUploadDelay();

// Returns whether to force the upload to fail.
bool ShouldForceUploadFailure();

// Whether to enable compact mode.
bool IsComposeboxCompactModeEnabled();

// Whether to force the composebox on top.
bool IsComposeboxForceTopEnabled();

// Used to enable the compact "one line" mode in the composebox.
BASE_DECLARE_FEATURE(kComposeboxCompactMode);

// Used to force top input plate in the composebox.
BASE_DECLARE_FEATURE(kComposeboxForceTop);

// Used to enable the AIM nudge button in the composebox.
BASE_DECLARE_FEATURE(kComposeboxAIMNudge);

// Used to show the title in the + button menu of the composebox.
BASE_DECLARE_FEATURE(kComposeboxMenuTitle);

// Determines if the persistent re-enable AIM button stays visible after the
// user exits the session.
bool IsComposeboxAIMNudgeEnabled();

// Whether the composebox + menu should show the title.
bool IsComposeboxMenuTitleEnabled();

// Used to check if we should display contextual suggestions for an image
// attachment.
BASE_DECLARE_FEATURE(kComposeboxFetchContextualSuggestionsForImage);

// Whether or not we should display contextual suggestions for an image.
bool IsComposeboxFetchContextualSuggestionsForImageEnabled();

// Used to check if we should display contextual suggestions for multiple
// attachments.
BASE_DECLARE_FEATURE(
    kComposeboxFetchContextualSuggestionsForMultipleAttachments);

// Whether or not we should display contextual suggestions for multiple
// attachments;
bool IsComposeboxFetchContextualSuggestionsForMultiAttachmentsEnabled();

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_FEATURES_H_
