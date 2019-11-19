// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MODALITY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MODALITY_H_

// An OverlayManager instance manages overlays for a single Browser at a single
// level of modality.  Additional values should be added for each desired level
// of modality (e.g. Browser-level modality, etc.).
enum class OverlayModality {
  // Used to schedule overlay UI to be displayed modally over a WebState's
  // content area (i.e. present UI requested by script execution on a page).
  kWebContentArea,
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MODALITY_H_
