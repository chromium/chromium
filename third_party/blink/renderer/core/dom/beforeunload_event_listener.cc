// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/beforeunload_event_listener.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"

namespace blink {

BeforeUnloadEventListener::BeforeUnloadEventListener(Document* document)
    : doc_(document) {}

void BeforeUnloadEventListener::Invoke(ExecutionContext* execution_context,
                                       Event* event) {
  DCHECK_EQ(event->type(), event_type_names::kBeforeunload);
  if (show_dialog_)
    To<BeforeUnloadEvent>(event)->preventDefault();
}

void BeforeUnloadEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(doc_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink
