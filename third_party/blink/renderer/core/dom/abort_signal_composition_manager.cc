// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal_composition_manager.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

AbortSignalCompositionManager::AbortSignalCompositionManager(
    AbortSignal& signal,
    AbortSignalCompositionType type)
    : signal_(signal), composition_type_(type) {
  CHECK(signal_);
}

AbortSignalCompositionManager::~AbortSignalCompositionManager() = default;

void AbortSignalCompositionManager::Trace(Visitor* visitor) const {
  visitor->Trace(signal_);
}

void AbortSignalCompositionManager::Settle() {
  DCHECK(!is_settled_);
  is_settled_ = true;

  signal_->OnSignalSettled(composition_type_);
}

DependentSignalCompositionManager::DependentSignalCompositionManager(
    AbortSignal& managed_signal,
    AbortSignalCompositionType type,
    const HeapVector<Member<AbortSignal>>& source_signals)
    : AbortSignalCompositionManager(managed_signal, type) {
  DCHECK(GetSignal().IsCompositeSignal());

  for (auto& source : source_signals) {
    if (source->IsCompositeSignal()) {
      auto* source_manager = To<DependentSignalCompositionManager>(
          source->GetCompositionManager(GetCompositionType()));
      DCHECK(source_manager);
      for (auto& signal : source_manager->GetSourceSignals()) {
        AddSourceSignal(*signal);
      }
    } else {
      AddSourceSignal(*source.Get());
    }
  }

  if (source_signals_.empty()) {
    Settle();
  }
}

DependentSignalCompositionManager::~DependentSignalCompositionManager() =
    default;

void DependentSignalCompositionManager::Trace(Visitor* visitor) const {
  AbortSignalCompositionManager::Trace(visitor);
  visitor->Trace(source_signals_);
}

void DependentSignalCompositionManager::AddSourceSignal(AbortSignal& source) {
  auto* source_manager = To<SourceSignalCompositionManager>(
      source.GetCompositionManager(GetCompositionType()));
  DCHECK(source_manager);
  // `source` won't emit `composition_type_` any longer, so there's no need to
  // follow. This can happen if `source` is associated with a GCed controller.
  if (source_manager->IsSettled()) {
    return;
  }

  DCHECK(!source.IsCompositeSignal());
  // Cycles are prevented by sources being specified only at creation time.
  DCHECK_NE(&GetSignal(), &source);

  // This can happen if the same signal gets passed to AbortSignal.any() more
  // than once, e.g. AbortSignal.any([signal, signal]).
  if (source_signals_.Contains(&source)) {
    return;
  }
  source_signals_.insert(&source);
  source_manager->AddDependentSignal(*this);
}

void DependentSignalCompositionManager::Settle() {
  AbortSignalCompositionManager::Settle();
  source_signals_.clear();
}

void DependentSignalCompositionManager::OnSourceSettled(
    SourceSignalCompositionManager& source_manager) {
  DCHECK(GetSignal().IsCompositeSignal());
  DCHECK(!IsSettled());

  // Note: `source_signals_` might not contain the source, and it might already
  // be empty if this source was removed during prefinalization. That's okay --
  // we only need to detect that the collection is empty on this path (if the
  // signal is being kept alive by the registry).
  source_signals_.erase(&source_manager.GetSignal());
  if (source_signals_.empty()) {
    Settle();
  }
}

SourceSignalCompositionManager::SourceSignalCompositionManager(
    AbortSignal& signal,
    AbortSignalCompositionType composition_type)
    : AbortSignalCompositionManager(signal, composition_type) {}

SourceSignalCompositionManager::~SourceSignalCompositionManager() = default;

void SourceSignalCompositionManager::Trace(Visitor* visitor) const {
  AbortSignalCompositionManager::Trace(visitor);
  visitor->Trace(dependent_signals_);
}

void SourceSignalCompositionManager::AddDependentSignal(
    DependentSignalCompositionManager& dependent_manager) {
  DCHECK(!IsSettled());
  DCHECK(!dependent_manager.IsSettled());
  DCHECK(dependent_manager.GetSignal().IsCompositeSignal());
  // New dependents should not be added to aborted signals.
  DCHECK(GetCompositionType() != AbortSignalCompositionType::kAbort ||
         !GetSignal().aborted());

  CHECK(&dependent_manager.GetSignal());
  dependent_signals_.insert(&dependent_manager.GetSignal());
}

void SourceSignalCompositionManager::Settle() {
  AbortSignalCompositionManager::Settle();

  for (auto& signal : dependent_signals_) {
    auto* manager = To<DependentSignalCompositionManager>(
        signal->GetCompositionManager(GetCompositionType()));
    DCHECK(manager);
    // The signal might have been settled if its `source_signals_` were cleared
    // during prefinalization and another source already notified it, or if the
    // signal was aborted.
    if (manager->IsSettled()) {
      continue;
    }
    manager->OnSourceSettled(*this);
  }
  dependent_signals_.clear();
}

}  // namespace blink
