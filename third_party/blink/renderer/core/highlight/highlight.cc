// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

namespace blink {

Highlight* Highlight::Create(const HeapVector<Member<AbstractRange>>& ranges) {
  return MakeGarbageCollected<Highlight>(ranges);
}

Highlight::Highlight(const HeapVector<Member<AbstractRange>>& ranges) {
  for (const auto& range : ranges)
    highlight_ranges_.insert(range);
}

Highlight::~Highlight() = default;

void Highlight::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_ranges_);
  visitor->Trace(containing_highlight_registries_);
  EventTarget::Trace(visitor);
}

void Highlight::ScheduleRepaintsInContainingHighlightRegistries() const {
  for (const auto& entry : containing_highlight_registries_) {
    DCHECK_GT(entry.value, 0u);
    Member<HighlightRegistry> highlight_registry = entry.key;
    highlight_registry->ScheduleRepaint();
  }
}

Highlight* Highlight::addForBinding(ScriptState*,
                                    AbstractRange* range,
                                    ExceptionState&) {
  if (highlight_ranges_.insert(range).is_new_entry) {
    ScheduleRepaintsInContainingHighlightRegistries();
  }
  return this;
}

void Highlight::clearForBinding(ScriptState*, ExceptionState&) {
  highlight_ranges_.clear();
  ScheduleRepaintsInContainingHighlightRegistries();
}

bool Highlight::deleteForBinding(ScriptState*,
                                 AbstractRange* range,
                                 ExceptionState&) {
  auto iterator = highlight_ranges_.find(range);
  if (iterator != highlight_ranges_.end()) {
    highlight_ranges_.erase(iterator);
    ScheduleRepaintsInContainingHighlightRegistries();
    return true;
  }
  return false;
}

bool Highlight::hasForBinding(ScriptState*,
                              AbstractRange* range,
                              ExceptionState&) const {
  return Contains(range);
}

wtf_size_t Highlight::size() const {
  return highlight_ranges_.size();
}

void Highlight::setPriority(const int32_t& priority) {
  priority_ = priority;
  ScheduleRepaintsInContainingHighlightRegistries();
}

bool Highlight::Contains(AbstractRange* range) const {
  return highlight_ranges_.Contains(range);
}

const AtomicString& Highlight::InterfaceName() const {
  // TODO(crbug.com/1346693)
  NOTIMPLEMENTED();
  return g_null_atom;
}

ExecutionContext* Highlight::GetExecutionContext() const {
  // TODO(crbug.com/1346693)
  NOTIMPLEMENTED();
  return nullptr;
}

void Highlight::RegisterIn(HighlightRegistry* highlight_registry) {
  auto map_iterator = containing_highlight_registries_.find(highlight_registry);
  if (map_iterator == containing_highlight_registries_.end()) {
    containing_highlight_registries_.insert(highlight_registry, 1);
  } else {
    DCHECK_GT(map_iterator->value, 0u);
    map_iterator->value++;
  }
}

void Highlight::DeregisterFrom(HighlightRegistry* highlight_registry) {
  auto map_iterator = containing_highlight_registries_.find(highlight_registry);
  CHECK_NE(map_iterator, containing_highlight_registries_.end(),
           base::NotFatalUntil::M130);
  DCHECK_GT(map_iterator->value, 0u);
  if (--map_iterator->value == 0)
    containing_highlight_registries_.erase(map_iterator);
}

Highlight::IterationSource::IterationSource(const Highlight& highlight)
    : index_(0) {
  highlight_ranges_snapshot_.ReserveInitialCapacity(
      highlight.highlight_ranges_.size());
  for (const auto& range : highlight.highlight_ranges_) {
    highlight_ranges_snapshot_.push_back(range);
  }
}

bool Highlight::IterationSource::FetchNextItem(ScriptState*,
                                               AbstractRange*& value,
                                               ExceptionState&) {
  if (index_ >= highlight_ranges_snapshot_.size())
    return false;
  value = highlight_ranges_snapshot_[index_++];
  return true;
}

void Highlight::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_ranges_snapshot_);
  HighlightSetIterable::IterationSource::Trace(visitor);
}

HighlightSetIterable::IterationSource* Highlight::CreateIterationSource(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<IterationSource>(*this);
}

}  // namespace blink
