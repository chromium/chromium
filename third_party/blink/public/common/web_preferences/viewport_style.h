// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_VIEWPORT_STYLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_VIEWPORT_STYLE_H_

namespace blink {
namespace web_pref {

// UA style if viewport is enabled.
enum class ViewportStyle {
  kDefault,
  // Includes viewportAndroid.css.
  kMobile,
  // Includes viewportTelevision.css.
  kTelevision
};

}  // namespace web_pref
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_VIEWPORT_STYLE_H_
