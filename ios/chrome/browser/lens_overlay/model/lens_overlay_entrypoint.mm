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
  }
}

}  // namespace lens
