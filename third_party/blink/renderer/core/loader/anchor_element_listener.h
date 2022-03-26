// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_LISTENER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"

namespace blink {

class Node;
class Event;
class HTMLAnchorElement;
class KURL;
class ExecutionContext;

// Listens for kPointerdown events, and checks to see if an anchor
// element is clicked with a valid href to be eligible for preloading.
class CORE_EXPORT AnchorElementListener : public NativeEventListener {
 public:
  explicit AnchorElementListener(
      base::RepeatingCallback<void(const KURL&)> callback);

  void Invoke(ExecutionContext* execution_context, Event* event) override;

 private:
  HTMLAnchorElement* FirstAnchorElementIncludingSelf(Node* node);

  // Gets the `html_anchor_element's` href attribute if it is part
  // of the HTTP family
  KURL GetHrefEligibleForPreloading(
      const HTMLAnchorElement& html_anchor_element);

  base::RepeatingCallback<void(const KURL&)> tracker_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_LISTENER_H_
