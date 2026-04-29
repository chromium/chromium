// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight.h"

#include "base/notimplemented.h"
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
  visitor->Trace(active_iterators_);
  visitor->Trace(containing_highlight_registries_);
  ScriptWrappable::Trace(visitor);
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
  NotifyIteratorsWillClear();
  highlight_ranges_.clear();
  ScheduleRepaintsInContainingHighlightRegistries();
}

bool Highlight::deleteForBinding(ScriptState*,
                                 AbstractRange* range,
                                 ExceptionState&) {
  auto iterator = highlight_ranges_.find(range);
  if (iterator != highlight_ranges_.end()) {
    NotifyIteratorsWillRemoveItem(range);
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
  CHECK_NE(map_iterator, containing_highlight_registries_.end());
  DCHECK_GT(map_iterator->value, 0u);
  if (--map_iterator->value == 0)
    containing_highlight_registries_.erase(map_iterator);
}

Highlight::IterationSource::IterationSource(Highlight& highlight)
    : highlight_(&highlight) {
  highlight.active_iterators_.insert(this);
}

bool Highlight::IterationSource::FetchNextItem(ScriptState*,
                                               AbstractRange*& value) {
  AbstractRange* entry = AdvanceAndGetNext(highlight_->highlight_ranges_,
                                           highlight_->active_iterators_);
  if (!entry) {
    return false;
  }
  value = entry;
  return true;
}

void Highlight::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_);
  HighlightLiveIterator::Trace(visitor);
  HighlightSetIterable::IterationSource::Trace(visitor);
}

void Highlight::NotifyIteratorsWillRemoveItem(AbstractRange* range) {
  for (auto& iter : active_iterators_) {
    if (iter) {
      iter->WillRemoveEntry(range, highlight_ranges_);
    }
  }
}

void Highlight::NotifyIteratorsWillClear() {
  for (auto& iter : active_iterators_) {
    if (iter) {
      iter->WillClear();
    }
  }
}

HighlightSetIterable::IterationSource* Highlight::CreateIterationSource(
    ScriptState*) {
  return MakeGarbageCollected<IterationSource>(*this);
}

}  // namespace blink
