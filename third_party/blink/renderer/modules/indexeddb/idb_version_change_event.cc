/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"

#include "third_party/blink/renderer/modules/indexed_db_names.h"

namespace blink {

IDBVersionChangeEvent::IDBVersionChangeEvent()
    : data_loss_(mojom::IDBDataLoss::None) {}

IDBVersionChangeEvent::IDBVersionChangeEvent(
    const AtomicString& event_type,
    uint64_t old_version,
    const std::optional<uint64_t>& new_version,
    mojom::IDBDataLoss data_loss,
    const String& data_loss_message)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      old_version_(old_version),
      new_version_(new_version),
      data_loss_(data_loss),
      data_loss_message_(data_loss_message) {}

IDBVersionChangeEvent::IDBVersionChangeEvent(
    const AtomicString& event_type,
    const IDBVersionChangeEventInit* initializer)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      old_version_(initializer->oldVersion()),
      data_loss_(mojom::IDBDataLoss::None) {
  if (initializer->hasNewVersion())
    new_version_ = initializer->newVersion();
  if (initializer->dataLoss() == "total")
    data_loss_ = mojom::IDBDataLoss::Total;
}

const AtomicString& IDBVersionChangeEvent::dataLoss() const {
  if (data_loss_ == mojom::IDBDataLoss::Total)
    return indexed_db_names::kTotal;
  return indexed_db_names::kNone;
}

const AtomicString& IDBVersionChangeEvent::InterfaceName() const {
  return event_interface_names::kIDBVersionChangeEvent;
}

void IDBVersionChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
