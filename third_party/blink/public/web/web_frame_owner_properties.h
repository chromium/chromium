// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_OWNER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_OWNER_PROPERTIES_H_

#include "third_party/blink/public/platform/web_string.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"  // nogncheck
#endif

namespace blink {

struct WebFrameOwnerProperties {
  enum class ScrollingMode { kAuto, kAlwaysOff, kAlwaysOn, kLast = kAlwaysOn };

  WebString name;  // browsing context container's name
  ScrollingMode scrolling_mode;
  int margin_width;
  int margin_height;
  bool allow_fullscreen;
  bool allow_payment_request;
  bool is_display_none;
  WebString required_csp;

 public:
  WebFrameOwnerProperties()
      : scrolling_mode(ScrollingMode::kAuto),
        margin_width(-1),
        margin_height(-1),
        allow_fullscreen(false),
        allow_payment_request(false),
        is_display_none(false) {}

#if INSIDE_BLINK
  WebFrameOwnerProperties(const WebString& name,
                          ScrollbarMode scrolling_mode,
                          int margin_width,
                          int margin_height,
                          bool allow_fullscreen,
                          bool allow_payment_request,
                          bool is_display_none,
                          const WebString& required_csp)
      : name(name),
        scrolling_mode(static_cast<ScrollingMode>(scrolling_mode)),
        margin_width(margin_width),
        margin_height(margin_height),
        allow_fullscreen(allow_fullscreen),
        allow_payment_request(allow_payment_request),
        is_display_none(is_display_none),
        required_csp(required_csp) {}
#endif
};

}  // namespace blink

#endif
