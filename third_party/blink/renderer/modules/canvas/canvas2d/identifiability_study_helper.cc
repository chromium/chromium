// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"

#include <cstdint>
#include <initializer_list>

#include "base/containers/span.h"
#include "base/hash/legacy_hash.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// The maximum number of canvas context operations to incorportate into digest
// computation -- constant, but may be overridden by tests using
// IdentifiabilityStudyHelper::ScopedMaxOperationsSetter.
/*static*/ int IdentifiabilityStudyHelper::max_operations_ = 1 << 20;

void IdentifiabilityStudyHelper::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

void IdentifiabilityStudyHelper::AddTokens(
    std::initializer_list<IdentifiableToken> tokens) {
  for (IdentifiableToken token : tokens) {
    partial_[position_++] = token.ToUkmMetricValue();
    if (position_ == 8) {
      chaining_value_ = DigestPartialData();
      position_ = 0;
    }
  }
}

uint64_t IdentifiabilityStudyHelper::DigestPartialData() const {
  return base::legacy::CityHash64WithSeed(
      base::make_span(
          reinterpret_cast<const uint8_t*>(partial_.data()),
          reinterpret_cast<const uint8_t*>(partial_.data() + position_)),
      chaining_value_);
}

}  // namespace blink
