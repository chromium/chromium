// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"

namespace blink {

AddEventListenerOptionsResolved::AddEventListenerOptionsResolved()
    : passive_forced_for_document_target_(false), passive_specified_(false) {}

AddEventListenerOptionsResolved::AddEventListenerOptionsResolved(
    const AddEventListenerOptions* options)
    : passive_forced_for_document_target_(false), passive_specified_(false) {
  DCHECK(options);
  // AddEventListenerOptions
  if (options->hasPassive())
    setPassive(options->passive());
  if (options->hasOnce())
    setOnce(options->once());
  if (options->hasSignal())
    setSignal(options->signal());
  // EventListenerOptions
  if (options->hasCapture())
    setCapture(options->capture());
}

AddEventListenerOptionsResolved::~AddEventListenerOptionsResolved() = default;

void AddEventListenerOptionsResolved::Trace(Visitor* visitor) const {
  AddEventListenerOptions::Trace(visitor);
}

}  // namespace blink
