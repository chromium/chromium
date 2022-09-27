// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_exit_fullscreen_observer.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"

namespace blink {
XrExitFullscreenObserver::XrExitFullscreenObserver() {
  DVLOG(2) << __func__;
}

XrExitFullscreenObserver::~XrExitFullscreenObserver() = default;

void XrExitFullscreenObserver::Invoke(ExecutionContext* execution_context,
                                      Event* event) {
  DVLOG(2) << __func__ << ": event type=" << event->type();

  document_->removeEventListener(event_type_names::kFullscreenchange, this,
                                 true);

  if (event->type() == event_type_names::kFullscreenchange) {
    // Succeeded, proceed with session shutdown. Expanding into the fullscreen
    // cutout is only valid for fullscreen mode which we just exited (cf.
    // MediaControlsDisplayCutoutDelegate::DidExitFullscreen), so we can
    // unconditionally turn this off here.
    document_->GetViewportData().SetExpandIntoDisplayCutout(false);
    std::move(on_exited_).Run();
  }
}

void XrExitFullscreenObserver::ExitFullscreen(Document* document,
                                              base::OnceClosure on_exited) {
  DVLOG(2) << __func__;
  document_ = document;
  on_exited_ = std::move(on_exited);

  document->addEventListener(event_type_names::kFullscreenchange, this, true);
  // "ua_originated" means that the browser process already exited
  // fullscreen. Set it to false because we need the browser process
  // to get notified that it needs to exit fullscreen. Use
  // FullyExitFullscreen to ensure that we return to non-fullscreen mode.
  // ExitFullscreen only unfullscreens a single element, potentially
  // leaving others in fullscreen mode.
  constexpr bool kUaOriginated = false;

  Fullscreen::FullyExitFullscreen(*document, kUaOriginated);
}

void XrExitFullscreenObserver::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  NativeEventListener::Trace(visitor);
}
}  // namespace blink
