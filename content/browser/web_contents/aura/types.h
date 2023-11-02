// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_TYPES_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_TYPES_H_

#include "content/browser/renderer_host/overscroll_controller.h"

namespace content {

enum class NavigationDirection {
  NONE,
  FORWARD,
  BACK,
  RELOAD,
  NAVIGATION_COUNT,
};

// Note that this enum is used to back an UMA histogram, so it should be
// treated as append-only.
enum UmaNavigationType {
  NAVIGATION_TYPE_NONE,
  FORWARD_TOUCHPAD,
  BACK_TOUCHPAD,
  FORWARD_TOUCHSCREEN,
  BACK_TOUCHSCREEN,
  RELOAD_TOUCHPAD,
  RELOAD_TOUCHSCREEN,
  NAVIGATION_TYPE_COUNT,
};

UmaNavigationType GetUmaNavigationType(NavigationDirection direction,
                                       OverscrollSource source);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_TYPES_H_
