// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_query.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"

namespace blink {

bool AnchorQuery::operator==(const AnchorQuery& other) const {
  return query_type_ == other.query_type_ && percentage_ == other.percentage_ &&
         base::ValuesEquivalent(anchor_specifier_, other.anchor_specifier_) &&
         value_ == other.value_;
}

void AnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_specifier_);
}

}  // namespace blink
