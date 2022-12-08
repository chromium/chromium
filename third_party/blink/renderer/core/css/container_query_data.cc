// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_data.h"

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

namespace blink {

void ContainerQueryData::Trace(Visitor* visitor) const {
  visitor->Trace(container_query_evaluator_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
