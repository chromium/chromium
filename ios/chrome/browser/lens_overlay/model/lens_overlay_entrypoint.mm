// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"

namespace lens {

LensOverlayInvocationSource InvocationSourceFromEntrypoint(
    LensOverlayEntrypoint entrypoint) {
  switch (entrypoint) {
    case LensOverlayEntrypoint::kLocationBar:
      return LensOverlayInvocationSource::kOmnibox;
    case LensOverlayEntrypoint::kOverflowMenu:
      return LensOverlayInvocationSource::kAppMenu;
  }
}

}  // namespace lens
