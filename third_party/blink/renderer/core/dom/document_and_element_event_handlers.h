// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_AND_ELEMENT_EVENT_HANDLERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_AND_ELEMENT_EVENT_HANDLERS_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DocumentAndElementEventHandlers {
  STATIC_ONLY(DocumentAndElementEventHandlers);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(copy, kCopy)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cut, kCut)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(paste, kPaste)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_AND_ELEMENT_EVENT_HANDLERS_H_
