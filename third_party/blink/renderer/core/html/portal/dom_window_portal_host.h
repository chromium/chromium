// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOM_WINDOW_PORTAL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOM_WINDOW_PORTAL_HOST_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class LocalDOMWindow;
class PortalHost;

class CORE_EXPORT DOMWindowPortalHost {
 public:
  static PortalHost* portalHost(LocalDOMWindow& window);
  static bool ShouldExposePortalHost(const LocalDOMWindow& window);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOM_WINDOW_PORTAL_HOST_H_
