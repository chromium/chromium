// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/test_runners.h"

#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/rust/bindings/test/cpp/associated_services.h"
#include "mojo/public/rust/bindings/test/test_util/bindings_unittests.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is for functions which contain testing logic;
// that is, they run tests themselves, and are called from tests.rs.
//
// This is needed because we need to test interop, which means ensuring
// that C++ can call Rusty things when necessary.

namespace bindings_unittests::mojom {

// Creates a MathService remote from the given handle, and verifies
// that it works by calling `Add`.
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

// Tests sending a malformed message from a C++ associated remote across a C++
// primary pipe to a Rust associated receiver.
void TestBadMessageToRustReceiver() {
  mojo::Remote<AssociatedSender> remote;
  mojo::PendingReceiver<AssociatedSender> receiver =
      remote.BindNewPipeAndPassReceiver();
  auto wrapper = std::make_unique<mojo::rust::ScopedMessagePipeHandleWrapper>(
      receiver.PassPipe());
  CreateAssociatedSenderInteropTest(std::move(wrapper));

  mojo::PendingAssociatedRemote<MathService> pending_remote;
  mojo::PendingAssociatedReceiver<MathService> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();
  remote->SendReceiver(std::move(pending_receiver));

  mojo::AssociatedRemote<MathService> math_remote(std::move(pending_remote));
  uint32_t bad_payload[6] = {
      0,  // num_bytes
      0,  // version
      0,  // interface_id
      0,  // method ordinal
      0,  // flags
      0,  // request_id
  };
  base::span<const uint8_t> payload_span =
      base::as_bytes(base::span(bad_payload));
  std::vector<mojo::ScopedHandle> empty_handles;
  base::span<mojo::ScopedHandle> handles_span(empty_handles);
  mojo::Message message(payload_span, handles_span);
  math_remote.internal_state()->endpoint_client_for_test()->SendMessage(
      &message, false);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace bindings_unittests::mojom
