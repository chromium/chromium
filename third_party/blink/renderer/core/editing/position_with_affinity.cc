// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file./*

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"

namespace blink {

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate(
    const PositionTemplate<Strategy>& position,
    TextAffinity affinity)
    : position_(position), affinity_(affinity) {}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate()
    : affinity_(TextAffinity::kDownstream) {}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate(
    const PositionTemplate<Strategy>& position)
    : position_(position), affinity_(TextAffinity::kDownstream) {}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::~PositionWithAffinityTemplate() =
    default;

template <typename Strategy>
void PositionWithAffinityTemplate<Strategy>::Trace(Visitor* visitor) const {
  visitor->Trace(position_);
}

template <typename Strategy>
bool PositionWithAffinityTemplate<Strategy>::operator==(
    const PositionWithAffinityTemplate& other) const {
  if (IsNull())
    return other.IsNull();
  return affinity_ == other.affinity_ && position_ == other.position_;
}

PositionWithAffinity ToPositionInDOMTreeWithAffinity(
    const PositionWithAffinity& position) {
  return position;
}

PositionWithAffinity ToPositionInDOMTreeWithAffinity(
    const PositionInFlatTreeWithAffinity& position) {
  return PositionWithAffinity(ToPositionInDOMTree(position.GetPosition()),
                              position.Affinity());
}

PositionInFlatTreeWithAffinity ToPositionInFlatTreeWithAffinity(
    const PositionWithAffinity& position) {
  return PositionInFlatTreeWithAffinity(
      ToPositionInFlatTree(position.GetPosition()), position.Affinity());
}

PositionInFlatTreeWithAffinity ToPositionInFlatTreeWithAffinity(
    const PositionInFlatTreeWithAffinity& position) {
  return position;
}

template class CORE_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const PositionWithAffinity& position_with_affinity) {
  return ostream << position_with_affinity.GetPosition() << '/'
                 << position_with_affinity.Affinity();
}

std::ostream& operator<<(
    std::ostream& ostream,
    const PositionInFlatTreeWithAffinity& position_with_affinity) {
  return ostream << position_with_affinity.GetPosition() << '/'
                 << position_with_affinity.Affinity();
}

}  // namespace blink
