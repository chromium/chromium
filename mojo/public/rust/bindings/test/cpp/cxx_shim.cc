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
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<MathService> receiver(wrapper->take_handle());
  return std::make_unique<PlusSevenMathService>(std::move(receiver));
}

// To be called by the Rust testing code. Constructs a math service remote from
// the given handle, and sends several messages through to ensure basic
// functionality.
void TestRemoteFromCpp(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::Remote<MathService> remote(
      mojo::PendingRemote<MathService>(wrapper->take_handle(), 0));

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

void CreatePlusSevenMathServiceAndRemote(
    std::unique_ptr<PlusSevenMathService>& service_out,
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper>& remote_out) {
  mojo::PendingRemote<MathService> remote;
  service_out = std::make_unique<PlusSevenMathService>(
      remote.InitWithNewPipeAndPassReceiver());
  remote_out = std::make_unique<mojo::rust::ScopedMessagePipeHandleWrapper>(
      remote.PassPipe());
}

}  // namespace bindings_unittests::mojom
