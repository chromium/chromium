// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_ENTRYPOINT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_ENTRYPOINT_H_

#import "components/lens/lens_overlay_invocation_source.h"

/// Lens overlay entrypoints on iOS.
enum class LensOverlayEntrypoint {
  // Omnibox location bar.
  kLocationBar,
  // 3-dots overflow menu.
  kOverflowMenu,
};

namespace lens {

/// Returns the LensOverlayInvocationSource equivalent of the
/// LensOverlayEntrypoint.
LensOverlayInvocationSource InvocationSourceFromEntrypoint(
    LensOverlayEntrypoint entrypoint);

}  // namespace lens

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_ENTRYPOINT_H_
