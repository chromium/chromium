// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/cxx_shim.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bindings_unittests::mojom {

std::unique_ptr<PlusSevenMathService> CreatePlusSevenMathService(
    MojoHandle handle) {
  mojo::PendingReceiver<MathService> receiver{
      mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle))};
  return std::make_unique<PlusSevenMathService>(std::move(receiver));
}

// To be called by the Rust testing code. Constructs a math service remote from
// the given handle, and sends several messages through to ensure basic
// functionality.
void TestRemoteFromCpp(MojoHandle handle) {
  mojo::Remote<MathService> remote(mojo::PendingRemote<MathService>(
      mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle)), 0));

  base::RunLoop run_loop;

  remote->Add(1, 2,
              base::BindOnce([](uint32_t result) { EXPECT_EQ(result, 3u); }));

  remote->AddTwoInts(TwoInts::New(7, 12),
                     base::BindOnce(
                         [](base::OnceClosure quit_closure, uint32_t result) {
                           EXPECT_EQ(result, 19u);
                           std::move(quit_closure).Run();
                         },
                         run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace bindings_unittests::mojom
