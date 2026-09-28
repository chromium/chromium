// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/test_runners.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
#include "mojo/public/rust/bindings/test/cpp/associated_services.h"
#include "mojo/public/rust/bindings/test/cpp/cxx_shim.h"
#include "mojo/public/rust/bindings/test/cxx.rs.h"
#include "mojo/public/rust/bindings/test/test_util/bindings_unittests.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is for functions which contain testing logic;
// that is, they run tests themselves, and are called from tests.rs.
//
// This is needed because we need to test interop, which means ensuring
// that C++ can call Rusty things when necessary.

namespace bindings_unittests::mojom {

using mojo::rust::bindings::MakeAssociatedEndpointRustAdapter;
using mojo::rust::bindings::PassPendingAssociatedRemote;

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

// Tests sending an associated receiver to Rust across a C++ primary remote,
// and verifies by calling `Add(500, 300)`.
void TestSendReceiverAndAddCppRemote(AssociatedSenderTestRemote& remote) {
  mojo::PendingAssociatedRemote<MathService> pending_remote;
  mojo::PendingAssociatedReceiver<MathService> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();

  auto adapter = MakeAssociatedEndpointRustAdapter(std::move(pending_receiver));
  remote.SendReceiver(std::move(adapter));

  mojo::AssociatedRemote<MathService> math_remote(std::move(pending_remote));
  base::RunLoop run_loop;
  math_remote->Add(500, 300,
                   base::BindOnce(
                       [](base::OnceClosure quit, uint32_t result) {
                         EXPECT_EQ(result, 807u);
                         std::move(quit).Run();
                       },
                       run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests requesting an associated remote from a Rust primary remote, and
// verifying by calling `Add(100, 200)`.
void TestRequestRemoteAndAddRustRemote(RustAssociatedSender& sender) {
  auto adapter = RequestRemote(sender);
  mojo::PendingAssociatedRemote<MathService> pending_remote =
      PassPendingAssociatedRemote<MathService>(std::move(adapter));

  mojo::AssociatedRemote<MathService> math_remote(std::move(pending_remote));
  base::RunLoop run_loop;
  math_remote->Add(100, 200,
                   base::BindOnce(
                       [](base::OnceClosure quit, uint32_t result) {
                         EXPECT_EQ(result, 307u);
                         std::move(quit).Run();
                       },
                       run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests sending an associated HandleService receiver to Rust across a C++
// primary remote, and verifies by calling `PassHandles` and sending a message
// over the passed message pipe.
void TestSendHandleReceiverAndPassHandlesCppRemote(
    AssociatedSenderTestRemote& remote) {
  mojo::PendingAssociatedRemote<HandleService> pending_remote;
  mojo::PendingAssociatedReceiver<HandleService> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();

  auto adapter = MakeAssociatedEndpointRustAdapter(std::move(pending_receiver));
  remote.SendHandleReceiver(std::move(adapter));

  mojo::AssociatedRemote<HandleService> handle_remote(
      std::move(pending_remote));
  mojo::PendingRemote<MathService> pending_math_remote;
  mojo::ScopedMessagePipeHandle math_receiver_handle =
      pending_math_remote.InitWithNewPipeAndPassReceiver().PassPipe();
  mojo::MessagePipe pipe2;
  mojo::MessagePipe pipe3;
  mojo::MessagePipe pipe4;
  handle_remote->PassHandles(
      std::move(math_receiver_handle), std::move(pipe2.handle0),
      std::move(pipe3.handle0),
      mojo::ScopedHandle::From(std::move(pipe4.handle0)));

  mojo::Remote<MathService> math_remote(std::move(pending_math_remote));
  base::RunLoop run_loop;
  math_remote->Add(500, 300,
                   base::BindOnce(
                       [](base::OnceClosure quit, uint32_t result) {
                         EXPECT_EQ(result, 807u);
                         std::move(quit).Run();
                       },
                       run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests requesting an associated HandleService remote from a Rust primary
// remote, and verifying by calling `PassHandles` and sending a message over
// the passed message pipe.
void TestRequestHandleRemoteAndPassHandlesRustRemote(
    RustAssociatedSender& sender) {
  auto adapter = RequestHandleRemote(sender);
  mojo::PendingAssociatedRemote<HandleService> pending_remote =
      PassPendingAssociatedRemote<HandleService>(std::move(adapter));

  mojo::AssociatedRemote<HandleService> handle_remote(
      std::move(pending_remote));
  mojo::PendingRemote<MathService> pending_math_remote;
  mojo::ScopedMessagePipeHandle math_receiver_handle =
      pending_math_remote.InitWithNewPipeAndPassReceiver().PassPipe();
  mojo::MessagePipe pipe2;
  mojo::MessagePipe pipe3;
  mojo::MessagePipe pipe4;
  handle_remote->PassHandles(
      std::move(math_receiver_handle), std::move(pipe2.handle0),
      std::move(pipe3.handle0),
      mojo::ScopedHandle::From(std::move(pipe4.handle0)));

  mojo::Remote<MathService> math_remote(std::move(pending_math_remote));
  base::RunLoop run_loop;
  math_remote->Add(100, 200,
                   base::BindOnce(
                       [](base::OnceClosure quit, uint32_t result) {
                         EXPECT_EQ(result, 307u);
                         std::move(quit).Run();
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

bool HaveSameGroupController(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint first,
    mojo::rust::bindings::CxxPendingAssociatedEndpoint second) {
  if (!first || !second) {
    return false;
  }
  mojo::ScopedInterfaceEndpointHandle first_handle = first->PassHandle();
  mojo::ScopedInterfaceEndpointHandle second_handle = second->PassHandle();
  return first_handle.group_controller() != nullptr &&
         first_handle.group_controller() == second_handle.group_controller();
}

}  // namespace bindings_unittests::mojom
