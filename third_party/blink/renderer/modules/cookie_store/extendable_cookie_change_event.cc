// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/extendable_cookie_change_event.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_list_item.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_cookie_change_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ExtendableCookieChangeEvent::~ExtendableCookieChangeEvent() = default;

const AtomicString& ExtendableCookieChangeEvent::InterfaceName() const {
  return event_interface_names::kExtendableCookieChangeEvent;
}

void ExtendableCookieChangeEvent::Trace(Visitor* visitor) const {
  ExtendableEvent::Trace(visitor);
  visitor->Trace(changed_);
  visitor->Trace(deleted_);
}

ExtendableCookieChangeEvent::ExtendableCookieChangeEvent(
    const AtomicString& type,
    HeapVector<Member<CookieListItem>> changed,
    HeapVector<Member<CookieListItem>> deleted,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, ExtendableEventInit::Create(), wait_until_observer),
      changed_(std::move(changed)),
      deleted_(std::move(deleted)) {}

ExtendableCookieChangeEvent::ExtendableCookieChangeEvent(
    const AtomicString& type,
    const ExtendableCookieChangeEventInit* initializer)
    : ExtendableEvent(type, initializer) {
  if (initializer->hasChanged())
    changed_ = initializer->changed();
  if (initializer->hasDeleted())
    deleted_ = initializer->deleted();
}

}  // namespace blink
