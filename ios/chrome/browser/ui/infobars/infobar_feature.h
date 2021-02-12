// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose whether to use OverlayPresenter to show the new Messages
// Infobar design. Use IsInfobarOverlayUIEnabled() instead of this constant
// directly.
extern const base::Feature kInfobarOverlayUI;

// Whether the Messages Infobar UI is presented using OverlayPresenter.
bool IsInfobarOverlayUIEnabled();

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_
