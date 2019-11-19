// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/navigator_user_media.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

namespace blink {

NavigatorUserMedia::NavigatorUserMedia(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      media_devices_(MakeGarbageCollected<MediaDevices>(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument()
                               : nullptr)) {}

const char NavigatorUserMedia::kSupplementName[] = "NavigatorUserMedia";

NavigatorUserMedia& NavigatorUserMedia::From(Navigator& navigator) {
  NavigatorUserMedia* supplement =
      Supplement<Navigator>::From<NavigatorUserMedia>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorUserMedia>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

MediaDevices* NavigatorUserMedia::GetMediaDevices() {
  return media_devices_;
}

MediaDevices* NavigatorUserMedia::mediaDevices(Navigator& navigator) {
  return NavigatorUserMedia::From(navigator).GetMediaDevices();
}

void NavigatorUserMedia::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_devices_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
