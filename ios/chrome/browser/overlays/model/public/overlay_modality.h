// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_MODALITY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_MODALITY_H_

// An OverlayManager instance manages overlays for a single Browser at a single
// level of modality.  Additional values should be added for each desired level
// of modality (e.g. Browser-level modality, etc.).
enum class OverlayModality {
  // Modality used for testing.
  kTesting,
  // Used to schedule overlay UI to be displayed modally over a WebState's
  // content area (i.e. present UI requested by script execution on a page).
  kWebContentArea,
  // Used to schedule overlay UI to display Infobar banners.  This modality is
  // in front of the Browser view, allowing the banner to be displayed over the
  // toolbar.
  kInfobarBanner,
  // Used to schedule overlay UI to display modal views for Infobars.  This
  // modality is in front of the banner, as it is possible for an Infobar to
  // present its modal on top of its banner.
  kInfobarModal,
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_MODALITY_H_
