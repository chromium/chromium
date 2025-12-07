// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"

namespace lens {

bool IsLVFEntrypoint(LensOverlayEntrypoint entrypoint) {
  return entrypoint == LensOverlayEntrypoint::kLVFCameraCapture ||
         entrypoint == LensOverlayEntrypoint::kLVFImagePicker;
}

bool IsImageContextMenuEntrypoint(LensOverlayEntrypoint entrypoint) {
  return entrypoint == LensOverlayEntrypoint::kSearchImageContextMenu;
}

bool EntrypointRequiresUserConsent(LensOverlayEntrypoint entrypoint) {
  switch (entrypoint) {
    case LensOverlayEntrypoint::kLocationBar:
    case LensOverlayEntrypoint::kOverflowMenu:
    case LensOverlayEntrypoint::kAIHub:
      return true;
    case LensOverlayEntrypoint::kSearchImageContextMenu:
    case LensOverlayEntrypoint::kLVFImagePicker:
    case LensOverlayEntrypoint::kLVFCameraCapture:
    case LensOverlayEntrypoint::kFREPromo:
      return false;
  }
}

LensOverlayInvocationSource InvocationSourceFromEntrypoint(
    LensOverlayEntrypoint entrypoint) {
  switch (entrypoint) {
    case LensOverlayEntrypoint::kLocationBar:
      return LensOverlayInvocationSource::kOmnibox;
    case LensOverlayEntrypoint::kOverflowMenu:
      return LensOverlayInvocationSource::kAppMenu;
    case LensOverlayEntrypoint::kSearchImageContextMenu:
      return LensOverlayInvocationSource::kContextMenu;
    case LensOverlayEntrypoint::kLVFImagePicker:
      return LensOverlayInvocationSource::kLVFGallery;
    case LensOverlayEntrypoint::kLVFCameraCapture:
      return LensOverlayInvocationSource::kLVFShutterButton;
    case LensOverlayEntrypoint::kAIHub:
      return LensOverlayInvocationSource::kAIHub;
    case LensOverlayEntrypoint::kFREPromo:
      return LensOverlayInvocationSource::kFREPromo;
  }
}

}  // namespace lens
