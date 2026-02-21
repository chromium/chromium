// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/add_seven_service.h"

namespace bindings_unittests::mojom {

PlusSevenMathService::PlusSevenMathService(
    mojo::PendingReceiver<MathService> receiver)
    : receiver_(this, std::move(receiver)) {}

PlusSevenMathService::~PlusSevenMathService() = default;

void PlusSevenMathService::Add(uint32_t a, uint32_t b, AddCallback callback) {
  std::move(callback).Run(a + b + 7);
}

void PlusSevenMathService::AddTwoInts(TwoIntsPtr ns,
                                      AddTwoIntsCallback callback) {
  // Too small to overflow!
  std::move(callback).Run(static_cast<uint32_t>(ns->a) +
                          static_cast<uint32_t>(ns->b) + 7);
}

}  // namespace bindings_unittests::mojom
