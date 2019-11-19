// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCES_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCES_CHANGE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event_init.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

class XRInputSourcesChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRInputSourcesChangeEvent* Create(
      const AtomicString& type,
      XRSession* session,
      const HeapVector<Member<XRInputSource>>& added,
      const HeapVector<Member<XRInputSource>>& removed) {
    return MakeGarbageCollected<XRInputSourcesChangeEvent>(type, session, added,
                                                           removed);
  }

  static XRInputSourcesChangeEvent* Create(
      const AtomicString& type,
      const XRInputSourcesChangeEventInit* initializer) {
    return MakeGarbageCollected<XRInputSourcesChangeEvent>(type, initializer);
  }

  XRInputSourcesChangeEvent(const AtomicString& type,
                            XRSession*,
                            const HeapVector<Member<XRInputSource>>&,
                            const HeapVector<Member<XRInputSource>>&);
  XRInputSourcesChangeEvent(const AtomicString&,
                            const XRInputSourcesChangeEventInit*);
  ~XRInputSourcesChangeEvent() override;

  XRSession* session() const { return session_; }
  const HeapVector<Member<XRInputSource>>& added() const { return added_; }
  const HeapVector<Member<XRInputSource>>& removed() const { return removed_; }

  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) override;

 private:
  Member<XRSession> session_;
  HeapVector<Member<XRInputSource>> added_;
  HeapVector<Member<XRInputSource>> removed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCES_CHANGE_EVENT_H_
