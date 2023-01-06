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
    : signal_(signal), composition_type_(type) {}

AbortSignalCompositionManager::~AbortSignalCompositionManager() = default;

void AbortSignalCompositionManager::Trace(Visitor* visitor) const {
  visitor->Trace(signal_);
}

DependentSignalCompositionManager::DependentSignalCompositionManager(
    AbortSignal& managed_signal,
    AbortSignalCompositionType type,
    HeapVector<Member<AbortSignal>>& source_signals)
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
}

DependentSignalCompositionManager::~DependentSignalCompositionManager() =
    default;

void DependentSignalCompositionManager::Trace(Visitor* visitor) const {
  AbortSignalCompositionManager::Trace(visitor);
  visitor->Trace(source_signals_);
}

void DependentSignalCompositionManager::AddSourceSignal(AbortSignal& source) {
  DCHECK(!source.IsCompositeSignal());
  // Internal signals can add dependent signals after construction via
  // AbortSignal::Follow, which would violate our assumptions for
  // AbortSignal.any().
  DCHECK_NE(source.GetSignalType(), AbortSignal::SignalType::kInternal);
  // Cycles are prevented by sources being specified only at creation time.
  DCHECK_NE(&GetSignal(), &source);

  // This can happen if the same signal gets passed to AbortSignal.any() more
  // than once, e.g. AbortSignal.any([signal, signal]).
  if (source_signals_.Contains(&source)) {
    return;
  }
  source_signals_.insert(&source);

  auto* source_manager = To<SourceSignalCompositionManager>(
      source.GetCompositionManager(GetCompositionType()));
  DCHECK(source_manager);
  source_manager->AddDependentSignal(*this);
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
  DCHECK(dependent_manager.GetSignal().IsCompositeSignal());
  // New dependents should not be added to aborted signals.
  DCHECK(GetCompositionType() != AbortSignalCompositionType::kAbort ||
         !GetSignal().aborted());

  dependent_signals_.insert(&dependent_manager.GetSignal());
}

}  // namespace blink
