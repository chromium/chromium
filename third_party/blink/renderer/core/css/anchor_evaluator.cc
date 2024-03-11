// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_evaluator.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"

namespace blink {

bool AnchorQuery::operator==(const AnchorQuery& other) const {
  return query_type_ == other.query_type_ && percentage_ == other.percentage_ &&
         base::ValuesEquivalent(anchor_specifier_, other.anchor_specifier_) &&
         value_ == other.value_;
}

unsigned AnchorQuery::GetHash() const {
  unsigned hash = 0;
  WTF::AddIntToHash(hash, WTF::HashInt(query_type_));
  WTF::AddIntToHash(hash, anchor_specifier_->GetHash());
  WTF::AddIntToHash(hash, WTF::HashFloat(percentage_));
  if (query_type_ == CSSAnchorQueryType::kAnchor) {
    WTF::AddIntToHash(hash, WTF::HashInt(absl::get<CSSAnchorValue>(value_)));
  } else {
    CHECK_EQ(query_type_, CSSAnchorQueryType::kAnchorSize);
    WTF::AddIntToHash(hash,
                      WTF::HashInt(absl::get<CSSAnchorSizeValue>(value_)));
  }
  return hash;
}

void AnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_specifier_);
}

}  // namespace blink
