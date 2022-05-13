// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include "third_party/blink/renderer/modules/ml/ml_context.h"

namespace blink {

MLOperand::MLOperand(MLContext* context) : ml_context_(context) {}

MLOperand::~MLOperand() = default;

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
