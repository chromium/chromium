// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_SHARED_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_SHARED_HELPER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class HTMLMediaElement;
class Element;

class MediaControlsSharedHelpers final {
  STATIC_ONLY(MediaControlsSharedHelpers);

 public:
  class TransitionEventListener final : public NativeEventListener {
   public:
    using Callback = base::RepeatingCallback<void()>;

    explicit TransitionEventListener(Element* element, Callback callback);
    void Attach();
    void Detach();
    bool IsAttached() const;
    void Invoke(ExecutionContext* context, Event* event) override;
    void Trace(blink::Visitor* visitor) override;

   private:
    bool attached_ = false;
    Callback callback_;
    Member<Element> element_;
  };

  static base::Optional<unsigned> GetCurrentBufferedTimeRange(
      HTMLMediaElement& media_element);

  static String FormatTime(double time);
  static bool ShouldShowFullscreenButton(const HTMLMediaElement& media_element);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_SHARED_HELPER_H_
