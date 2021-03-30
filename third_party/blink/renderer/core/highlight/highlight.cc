// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

namespace blink {

Highlight* Highlight::Create(const String& name,
                             HeapVector<Member<AbstractRange>>& ranges) {
  return MakeGarbageCollected<Highlight>(name, ranges);
}

Highlight::Highlight(const String& name,
                     HeapVector<Member<AbstractRange>>& ranges)
    : name_(name) {
  for (const auto& range : ranges)
    highlight_ranges_.insert(range);
}

Highlight::~Highlight() = default;

void Highlight::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_ranges_);
  ScriptWrappable::Trace(visitor);
}

Highlight* Highlight::addForBinding(ScriptState*,
                                    AbstractRange* range,
                                    ExceptionState&) {
  highlight_ranges_.insert(range);
  return this;
}

void Highlight::clearForBinding(ScriptState*, ExceptionState&) {
  highlight_ranges_.clear();
}

bool Highlight::deleteForBinding(ScriptState*,
                                 AbstractRange* range,
                                 ExceptionState&) {
  auto iterator = highlight_ranges_.find(range);
  if (iterator != highlight_ranges_.end()) {
    highlight_ranges_.erase(iterator);
    return true;
  }
  return false;
}

bool Highlight::hasForBinding(ScriptState*,
                              AbstractRange* range,
                              ExceptionState&) const {
  return highlight_ranges_.Contains(range);
}

wtf_size_t Highlight::size() const {
  return highlight_ranges_.size();
}

Highlight::IterationSource::IterationSource(const Highlight& highlight)
    : index_(0) {
  highlight_ranges_snapshot_.ReserveInitialCapacity(
      highlight.highlight_ranges_.size());
  for (const auto& range : highlight.highlight_ranges_) {
    highlight_ranges_snapshot_.push_back(range);
  }
}

bool Highlight::IterationSource::Next(ScriptState*,
                                      Member<AbstractRange>& key,
                                      Member<AbstractRange>& value,
                                      ExceptionState&) {
  if (index_ >= highlight_ranges_snapshot_.size())
    return false;
  key = value = highlight_ranges_snapshot_[index_++];
  return true;
}

void Highlight::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_ranges_snapshot_);
  HighlightSetIterable::IterationSource::Trace(visitor);
}

HighlightSetIterable::IterationSource* Highlight::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<IterationSource>(*this);
}

}  // namespace blink
