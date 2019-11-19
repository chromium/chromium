// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_PRESENTATION_STATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_PRESENTATION_STATE_H_

// Presentation states for InfobarBanner.
enum class InfobarBannerPresentationState {
  // The InfobarBanner is not presented and is not part of the view hierarchy.
  NotPresented = 0,
  // The InfobarBanner presentation is taking place. e.g. The presentation
  // is being animated and hasn't completed yet, this means that the banner is
  // interactable but not part of the view hierarcy yet.
  IsAnimating = 1,
  // The InfobarBanner is currently presented. e.g. The presentation has
  // finished and the InfobarBanner is now presented and part of the
  // view hierarchy.
  Presented = 2,
};

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_PRESENTATION_STATE_H_
