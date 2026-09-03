// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_add_event_listener_options.h"

namespace blink {

AddEventListenerOptionsResolved::AddEventListenerOptionsResolved(
    const AddEventListenerOptions* options) {
  DCHECK(options);
  // AddEventListenerOptions
  if (options->hasPassive())
    SetPassive(options->passive());
  if (options->hasOnce())
    SetOnce(options->once());
  if (options->hasSignal())
    SetSignal(options->signal());
  // EventListenerOptions
  if (options->hasCapture())
    SetCapture(options->capture());
}

}  // namespace blink
