// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/portal/portal_host.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

// static
PortalHost* DOMWindowPortalHost::portalHost(LocalDOMWindow& window) {
  if (ShouldExposePortalHost(window))
    return &PortalHost::From(window);
  return nullptr;
}

// static
bool DOMWindowPortalHost::ShouldExposePortalHost(const LocalDOMWindow& window) {
  // The portal host is only exposed in the main frame of a page
  // embedded in a portal.
  return window.GetFrame() && window.GetFrame()->IsMainFrame() &&
         window.GetFrame()->GetPage()->InsidePortal();
}

}  // namespace blink
