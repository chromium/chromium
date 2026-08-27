// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/types.h"

namespace content {

UmaNavigationType GetUmaNavigationType(NavigationDirection direction,
                                       OverscrollSource source) {
  if (direction == NavigationDirection::NONE ||
      source == OverscrollSource::NONE) {
    return NAVIGATION_TYPE_NONE;
  }
  if (direction == NavigationDirection::BACK) {
    return source == OverscrollSource::TOUCHPAD
               ? UmaNavigationType::BACK_TOUCHPAD
               : UmaNavigationType::BACK_TOUCHSCREEN;
  }
  if (direction == NavigationDirection::FORWARD) {
    return source == OverscrollSource::TOUCHPAD
               ? UmaNavigationType::FORWARD_TOUCHPAD
               : UmaNavigationType::FORWARD_TOUCHSCREEN;
  }
  DCHECK_EQ(direction, NavigationDirection::RELOAD);
  return source == OverscrollSource::TOUCHPAD
             ? UmaNavigationType::RELOAD_TOUCHPAD
             : UmaNavigationType::RELOAD_TOUCHSCREEN;
}

}  // namespace content
