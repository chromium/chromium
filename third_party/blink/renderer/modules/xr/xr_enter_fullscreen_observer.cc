// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_enter_fullscreen_observer.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen_request_type.h"
#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"

namespace blink {
XrEnterFullscreenObserver::XrEnterFullscreenObserver() {
  DVLOG(2) << __func__;
}

XrEnterFullscreenObserver::~XrEnterFullscreenObserver() = default;

void XrEnterFullscreenObserver::Invoke(ExecutionContext* execution_context,
                                       Event* event) {
  DVLOG(2) << __func__ << ": event type=" << event->type();

  // This handler should only be called once, it's unregistered after use.
  DCHECK(on_completed_);

  auto& doc = fullscreen_element_->GetDocument();

  doc.removeEventListener(event_type_names::kFullscreenchange, this, true);
  doc.removeEventListener(event_type_names::kFullscreenerror, this, true);

  if (event->type() == event_type_names::kFullscreenchange) {
    // Succeeded, force the content to expand all the way (because that's what
    // the XR content will do), and then notify of this success.
    doc.GetViewportData().SetExpandIntoDisplayCutout(true);
    std::move(on_completed_).Run(true);
  }
  if (event->type() == event_type_names::kFullscreenerror) {
    // Notify our callback that we failed to enter fullscreen.
    std::move(on_completed_).Run(false);
  }
}

void XrEnterFullscreenObserver::RequestFullscreen(
    Element* fullscreen_element,
    bool setup_for_dom_overlay,
    bool may_have_camera_access,
    base::OnceCallback<void(bool)> on_completed) {
  DCHECK(!on_completed_);
  DCHECK(fullscreen_element);
  on_completed_ = std::move(on_completed);
  fullscreen_element_ = fullscreen_element;

  // If we're already in fullscreen, there may be different options applied for
  // navigationUI than what we need. In order to avoid that, we should have
  // exited the fullscreen prior to attempting to enter it here.
  DCHECK(
      !Fullscreen::FullscreenElementFrom(fullscreen_element_->GetDocument()));

  // Set up event listeners for success and failure.
  fullscreen_element_->GetDocument().addEventListener(
      event_type_names::kFullscreenchange, this, true);
  fullscreen_element_->GetDocument().addEventListener(
      event_type_names::kFullscreenerror, this, true);

  // Use the event-generating unprefixed version of RequestFullscreen to ensure
  // that the fullscreen event listener is informed once this completes.
  FullscreenOptions* options = FullscreenOptions::Create();
  options->setNavigationUI("hide");

  // Grant fullscreen API permission for the following call. Requesting the
  // immersive session had required a user activation state, but that may have
  // expired by now due to the user taking time to respond to the consent
  // prompt.
  ScopedAllowFullscreen scope(setup_for_dom_overlay
                                  ? ScopedAllowFullscreen::kXrOverlay
                                  : ScopedAllowFullscreen::kXrSession);

  FullscreenRequestType request_type =
      may_have_camera_access ? FullscreenRequestType::kForXrArWithCamera
                             : FullscreenRequestType::kNull;
  if (setup_for_dom_overlay) {
    request_type = request_type | FullscreenRequestType::kForXrOverlay;
  }

  // Flow will continue in `XrEnterFullscreenObserver::Invoke()` when fullscreen
  // request completes (either successfully or errors out).
  Fullscreen::RequestFullscreen(*fullscreen_element_, options, request_type);
}

void XrEnterFullscreenObserver::Trace(Visitor* visitor) const {
  visitor->Trace(fullscreen_element_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink
