// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

HighlightRegistry* HighlightRegistry::From(LocalDOMWindow& window) {
  HighlightRegistry* supplement =
      Supplement<LocalDOMWindow>::From<HighlightRegistry>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<HighlightRegistry>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, supplement);
  }
  return supplement;
}

HighlightRegistry::HighlightRegistry(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

HighlightRegistry::~HighlightRegistry() = default;

const char HighlightRegistry::kSupplementName[] = "HighlightRegistry";

void HighlightRegistry::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

HighlightRegistry* HighlightRegistry::addForBinding(
    ScriptState*,
    Highlight* highlight,
    ExceptionState& exception_state) {
  if (!registered_highlight_names_.insert(highlight->name()).is_new_entry) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Cannot add a Highlight with the same name as an existing one.");
  } else {
    highlights_.insert(highlight);
  }

  return this;
}

void HighlightRegistry::clearForBinding(ScriptState*, ExceptionState&) {
  registered_highlight_names_.clear();
  highlights_.clear();
}

bool HighlightRegistry::deleteForBinding(ScriptState*,
                                         Highlight* highlight,
                                         ExceptionState&) {
  auto name_iterator = registered_highlight_names_.find(highlight->name());
  if (name_iterator != registered_highlight_names_.end()) {
    registered_highlight_names_.erase(name_iterator);
    highlights_.erase(highlight);
    return true;
  }

  return false;
}

bool HighlightRegistry::hasForBinding(ScriptState*,
                                      Highlight* highlight,
                                      ExceptionState&) const {
  return highlights_.Contains(highlight);
}

HighlightRegistry::IterationSource::IterationSource(
    const HighlightRegistry& highlight_registry)
    : index_(0) {
  highlights_snapshot_.ReserveInitialCapacity(
      highlight_registry.highlights_.size());
  for (const auto& highlight : highlight_registry.highlights_) {
    highlights_snapshot_.push_back(highlight);
  }
}

bool HighlightRegistry::IterationSource::Next(ScriptState*,
                                              Member<Highlight>& key,
                                              Member<Highlight>& value,
                                              ExceptionState&) {
  if (index_ >= highlights_snapshot_.size())
    return false;
  key = value = highlights_snapshot_[index_++];
  return true;
}

void HighlightRegistry::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_snapshot_);
  HighlightRegistrySetIterable::IterationSource::Trace(visitor);
}

HighlightRegistrySetIterable::IterationSource*
HighlightRegistry::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<IterationSource>(*this);
}

}  // namespace blink
