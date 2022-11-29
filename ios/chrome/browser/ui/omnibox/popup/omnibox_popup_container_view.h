// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CONTAINER_VIEW_H_

#include <UIKit/UIKit.h>

/// Container view for the popup view hierarchy. It overrides hit-testing so we
/// can dismiss hits on the PopupEmptySpaceView.
@interface OmniboxPopupContainerView : UIView
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CONTAINER_VIEW_H_
