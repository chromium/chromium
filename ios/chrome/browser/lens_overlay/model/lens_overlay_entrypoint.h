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
  // Search image button from context menu.
  kSearchImageContextMenu,
  // Image captured by LVF camera.
  kLVFCameraCapture,
  // Image retrieved from gallery.
  kLVFImagePicker,
  // The AI hub menu.
  kAIHub,
  // The Interactive Lens screen in the First Run Experience.
  kFREPromo,
};

namespace lens {

/// Whether the entrypoint corresponds to an LVF session.
bool IsLVFEntrypoint(LensOverlayEntrypoint entrypoint);

/// Whether the entrypoint corresponds to a context menu session.
bool IsImageContextMenuEntrypoint(LensOverlayEntrypoint entrypoint);

/// Whether the entrypoint requires user consent.
bool EntrypointRequiresUserConsent(LensOverlayEntrypoint entrypoint);

/// Returns the LensOverlayInvocationSource equivalent of the
/// LensOverlayEntrypoint.
LensOverlayInvocationSource InvocationSourceFromEntrypoint(
    LensOverlayEntrypoint entrypoint);

}  // namespace lens

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_ENTRYPOINT_H_
