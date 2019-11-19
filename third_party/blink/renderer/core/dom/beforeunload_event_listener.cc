// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/beforeunload_event_listener.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

BeforeUnloadEventListener::BeforeUnloadEventListener(Document* document)
    : doc_(document) {}

void BeforeUnloadEventListener::Invoke(ExecutionContext* execution_context,
                                       Event* event) {
  DCHECK_EQ(event->type(), event_type_names::kBeforeunload);
  if (show_dialog_)
    ToBeforeUnloadEvent(event)->setReturnValue(g_empty_string);
}

void BeforeUnloadEventListener::Trace(Visitor* visitor) {
  visitor->Trace(doc_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink
