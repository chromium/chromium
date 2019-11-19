/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/events/text_event.h"

#include "third_party/blink/renderer/core/dom/document_fragment.h"

namespace blink {

TextEvent* TextEvent::Create() {
  return MakeGarbageCollected<TextEvent>();
}

TextEvent* TextEvent::Create(AbstractView* view,
                             const String& data,
                             TextEventInputType input_type) {
  return MakeGarbageCollected<TextEvent>(view, data, input_type);
}

TextEvent* TextEvent::CreateForPlainTextPaste(AbstractView* view,
                                              const String& data,
                                              bool should_smart_replace) {
  return MakeGarbageCollected<TextEvent>(view, data, nullptr,
                                         should_smart_replace, false);
}

TextEvent* TextEvent::CreateForFragmentPaste(AbstractView* view,
                                             DocumentFragment* data,
                                             bool should_smart_replace,
                                             bool should_match_style) {
  return MakeGarbageCollected<TextEvent>(view, "", data, should_smart_replace,
                                         should_match_style);
}

TextEvent* TextEvent::CreateForDrop(AbstractView* view, const String& data) {
  return MakeGarbageCollected<TextEvent>(view, data, kTextEventInputDrop);
}

TextEvent::TextEvent()
    : input_type_(kTextEventInputKeyboard),
      should_smart_replace_(false),
      should_match_style_(false) {}

TextEvent::TextEvent(AbstractView* view,
                     const String& data,
                     TextEventInputType input_type)
    : UIEvent(event_type_names::kTextInput,
              Bubbles::kYes,
              Cancelable::kYes,
              ComposedMode::kComposed,
              base::TimeTicks::Now(),
              view,
              0,
              nullptr),
      input_type_(input_type),
      data_(data),
      pasting_fragment_(nullptr),
      should_smart_replace_(false),
      should_match_style_(false) {}

TextEvent::TextEvent(AbstractView* view,
                     const String& data,
                     DocumentFragment* pasting_fragment,
                     bool should_smart_replace,
                     bool should_match_style)
    : UIEvent(event_type_names::kTextInput,
              Bubbles::kYes,
              Cancelable::kYes,
              ComposedMode::kComposed,
              base::TimeTicks::Now(),
              view,
              0,
              nullptr),
      input_type_(kTextEventInputPaste),
      data_(data),
      pasting_fragment_(pasting_fragment),
      should_smart_replace_(should_smart_replace),
      should_match_style_(should_match_style) {}

TextEvent::~TextEvent() = default;

void TextEvent::initTextEvent(const AtomicString& type,
                              bool bubbles,
                              bool cancelable,
                              AbstractView* view,
                              const String& data) {
  if (IsBeingDispatched())
    return;

  initUIEvent(type, bubbles, cancelable, view, 0);

  data_ = data;
}

const AtomicString& TextEvent::InterfaceName() const {
  return event_interface_names::kTextEvent;
}

void TextEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(pasting_fragment_);
  UIEvent::Trace(visitor);
}

}  // namespace blink
