// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TYPE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TYPE_H_

// Enum describing the types of overlays displayed for a single infobar.
enum class InfobarOverlayType : short {
  // Used to create banner overlays that are displayed in
  // OverlayModality::kInfobarBanner.
  kBanner,
  // Used to create modal overlays that are displayed in
  // OverlayModality::kInfobarModal.
  kModal,
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TYPE_H_
