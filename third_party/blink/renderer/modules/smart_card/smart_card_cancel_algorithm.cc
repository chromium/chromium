// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_cancel_algorithm.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"

namespace blink {

SmartCardCancelAlgorithm::SmartCardCancelAlgorithm(
    SmartCardContext* blink_scard_context)
    : blink_scard_context_(blink_scard_context) {}

SmartCardCancelAlgorithm::~SmartCardCancelAlgorithm() = default;

void SmartCardCancelAlgorithm::Run() {
  blink_scard_context_->Cancel();
}

void SmartCardCancelAlgorithm::Trace(Visitor* visitor) const {
  visitor->Trace(blink_scard_context_);
  Algorithm::Trace(visitor);
}

}  // namespace blink
