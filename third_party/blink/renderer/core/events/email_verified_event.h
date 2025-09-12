// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EMAIL_VERIFIED_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EMAIL_VERIFIED_EVENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

class EmailVerifiedEventInit;

class CORE_EXPORT EmailVerifiedEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EmailVerifiedEvent* Create(const AtomicString& type,
                                    const AtomicString& presentation_token) {
    return MakeGarbageCollected<EmailVerifiedEvent>(type, presentation_token);
  }
  static EmailVerifiedEvent* Create(const AtomicString& type,
                                    const EmailVerifiedEventInit* initializer) {
    return MakeGarbageCollected<EmailVerifiedEvent>(type, initializer);
  }
  explicit EmailVerifiedEvent(const AtomicString& type,
                              const String& presentation_token);
  explicit EmailVerifiedEvent(const AtomicString& type,
                              const EmailVerifiedEventInit* initializer);
  ~EmailVerifiedEvent() override = default;

  const String& presentationToken() const { return presentation_token_; }

  const AtomicString& InterfaceName() const override;

 private:
  String presentation_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EMAIL_VERIFIED_EVENT_H_
