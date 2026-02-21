// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ADD_SEVEN_SERVICE_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ADD_SEVEN_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/rust/bindings/test/test_util/bindings_unittests.test-mojom.h"

namespace bindings_unittests::mojom {

class PlusSevenMathService : public MathService {
 public:
  explicit PlusSevenMathService(mojo::PendingReceiver<MathService> receiver);
  PlusSevenMathService(const PlusSevenMathService&) = delete;
  PlusSevenMathService& operator=(const PlusSevenMathService&) = delete;
  ~PlusSevenMathService() override;

  // MathService implementation:
  void Add(uint32_t a, uint32_t b, AddCallback callback) override;
  void AddTwoInts(TwoIntsPtr ns, AddTwoIntsCallback callback) override;

 private:
  // This class follows the standard C++ practice of holding its own receiver
  mojo::Receiver<MathService> receiver_;
};

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ADD_SEVEN_SERVICE_H_
