/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/style/filter_operations.h"

#include <numeric>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace blink {

FilterOperations::FilterOperations() = default;

void FilterOperations::Trace(Visitor* visitor) const {
  visitor->Trace(operations_);
}

bool FilterOperations::operator==(const FilterOperations& o) const {
  if (operations_.size() != o.operations_.size()) {
    return false;
  }

  unsigned s = operations_.size();
  for (unsigned i = 0; i < s; i++) {
    if (*operations_[i] != *o.operations_[i]) {
      return false;
    }
  }

  return true;
}

bool FilterOperations::CanInterpolateWith(const FilterOperations& other) const {
  auto can_interpolate = [](FilterOperation* operation) {
    return FilterOperation::CanInterpolate(operation->GetType());
  };
  if (!base::ranges::all_of(Operations(), can_interpolate) ||
      !base::ranges::all_of(other.Operations(), can_interpolate)) {
    return false;
  }

  wtf_size_t common_size =
      std::min(Operations().size(), other.Operations().size());
  for (wtf_size_t i = 0; i < common_size; ++i) {
    if (!Operations()[i]->IsSameType(*other.Operations()[i])) {
      return false;
    }
  }
  return true;
}

gfx::RectF FilterOperations::MapRect(const gfx::RectF& rect) const {
  auto accumulate_mapped_rect = [](const gfx::RectF& rect,
                                   const Member<FilterOperation>& op) {
    return op->MapRect(rect);
  };
  return std::accumulate(operations_.begin(), operations_.end(), rect,
                         accumulate_mapped_rect);
}

bool FilterOperations::HasFilterThatAffectsOpacity() const {
  return base::ranges::any_of(operations_, [](const auto& operation) {
    return operation->AffectsOpacity();
  });
}

bool FilterOperations::HasFilterThatMovesPixels() const {
  return base::ranges::any_of(operations_, [](const auto& operation) {
    return operation->MovesPixels();
  });
}

bool FilterOperations::HasReferenceFilter() const {
  return base::Contains(operations_, FilterOperation::OperationType::kReference,
                        &FilterOperation::GetType);
}

bool FilterOperations::UsesCurrentColor() const {
  return base::ranges::any_of(operations_, [](const auto& operation) {
    return operation->UsesCurrentColor();
  });
}

void FilterOperations::AddClient(SVGResourceClient& client) const {
  for (FilterOperation* operation : operations_) {
    if (operation->GetType() == FilterOperation::OperationType::kReference) {
      To<ReferenceFilterOperation>(*operation).AddClient(client);
    }
  }
}

void FilterOperations::RemoveClient(SVGResourceClient& client) const {
  for (FilterOperation* operation : operations_) {
    if (operation->GetType() == FilterOperation::OperationType::kReference) {
      To<ReferenceFilterOperation>(*operation).RemoveClient(client);
    }
  }
}

}  // namespace blink
