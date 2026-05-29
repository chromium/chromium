// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "mojo/public/cpp/bindings/tests/race_condition_scheduler.h"
#include "mojo/public/cpp/bindings/tests/router_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kTestInterfaceName[] = "TestInterface";

std::string_view GetPayloadAsStringView(const Message& msg) {
  const char* data = reinterpret_cast<const char*>(msg.payload());
  std::string_view view(data, msg.payload_num_bytes());
  // Trim trailing null characters if present.
  if (auto pos = view.find('\0'); pos != std::string_view::npos) {
    view = view.substr(0, pos);
  }
  return view;
}

IPCStableHashFunction MessageToMethodInfo(Message& message) {
  return nullptr;
}

const char* MessageToMethodName(Message& message) {
  return "method";
}

using mojo::internal::MultiplexRouter;
using mojo::test::RaceConditionScheduler;

class MultiplexRouterTest : public testing::Test {
 public:
  MultiplexRouterTest() {}

  void SetUp() override {
    MessagePipe pipe;
    router0_ = MultiplexRouter::CreateAndStartReceiving(
        std::move(pipe.handle0), MultiplexRouter::MULTI_INTERFACE, false,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    router1_ = MultiplexRouter::CreateAndStartReceiving(
        std::move(pipe.handle1), MultiplexRouter::MULTI_INTERFACE, true,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&endpoint0_,
                                                                &endpoint1_);
    auto id = router0_->AssociateInterface(std::move(endpoint1_));
    endpoint1_ = router1_->CreateLocalEndpointHandle(id);
  }

  void TearDown() override {
    endpoint1_.reset();
    endpoint0_.reset();
    router1_ = nullptr;
    router0_ = nullptr;
  }

 protected:
  scoped_refptr<MultiplexRouter> router0_;
  scoped_refptr<MultiplexRouter> router1_;
  ScopedInterfaceEndpointHandle endpoint0_;
  ScopedInterfaceEndpointHandle endpoint1_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MultiplexRouterTest, BasicRequestResponse) {
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);
  ResponseGenerator generator;
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator, std::make_unique<PassThroughFilter>(),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue,
                                                     run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string_view("hello world!"), GetPayloadAsStringView(response));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  base::RunLoop run_loop2;
  client0.AcceptWithResponder(
      &request2, std::make_unique<MessageAccumulator>(&message_queue,
                                                      run_loop2.QuitClosure()));

  run_loop2.Run();

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string_view("hello again world!"),
            GetPayloadAsStringView(response));
}

TEST_F(MultiplexRouterTest, BasicRequestResponse_Synchronous) {
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);
  ResponseGenerator generator;
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator, std::make_unique<PassThroughFilter>(),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue));

  router1_->WaitForIncomingMessage();
  router0_->WaitForIncomingMessage();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string_view("hello world!"), GetPayloadAsStringView(response));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  client0.AcceptWithResponder(
      &request2, std::make_unique<MessageAccumulator>(&message_queue));

  router1_->WaitForIncomingMessage();
  router0_->WaitForIncomingMessage();

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string_view("hello again world!"),
            GetPayloadAsStringView(response));
}

// Tests MultiplexRouter using the LazyResponseGenerator. The responses will not
// be sent until after the requests have been accepted.
TEST_F(MultiplexRouterTest, LazyResponses) {
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, base::WrapUnique(new PassThroughFilter()),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);
  base::RunLoop run_loop;
  LazyResponseGenerator generator(run_loop.QuitClosure());
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator,
      base::WrapUnique(new PassThroughFilter()), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop2;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue,
                                                     run_loop2.QuitClosure()));
  run_loop.Run();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the response.
  EXPECT_TRUE(generator.responder_is_valid());
  generator.CompleteWithResponse();
  run_loop2.Run();

  // Check the response.
  EXPECT_FALSE(message_queue.IsEmpty());
  Message response;
  message_queue.Pop(&response);
  EXPECT_EQ(std::string_view("hello world!"), GetPayloadAsStringView(response));

  // Send a second message on the pipe.
  base::RunLoop run_loop3;
  generator.set_closure(run_loop3.QuitClosure());
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  base::RunLoop run_loop4;
  client0.AcceptWithResponder(
      &request2, std::make_unique<MessageAccumulator>(&message_queue,
                                                      run_loop4.QuitClosure()));
  run_loop3.Run();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the second response.
  EXPECT_TRUE(generator.responder_is_valid());
  generator.CompleteWithResponse();
  run_loop4.Run();

  // Check the second response.
  EXPECT_FALSE(message_queue.IsEmpty());
  message_queue.Pop(&response);
  EXPECT_EQ(std::string_view("hello again world!"),
            GetPayloadAsStringView(response));
}

void ForwardErrorHandler(bool* called, base::OnceClosure callback) {
  *called = true;
  std::move(callback).Run();
}

// Tests that if the receiving application destroys the responder_ without
// sending a response, then we trigger connection error at both sides. Moreover,
// both sides still appear to have a valid message pipe handle bound.
TEST_F(MultiplexRouterTest, MissingResponses) {
  base::RunLoop run_loop0, run_loop1;
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, base::WrapUnique(new PassThroughFilter()),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);
  bool error_handler_called0 = false;
  client0.set_connection_error_handler(base::BindOnce(
      &ForwardErrorHandler, &error_handler_called0, run_loop0.QuitClosure()));

  base::RunLoop run_loop3;
  LazyResponseGenerator generator(run_loop3.QuitClosure());
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator,
      base::WrapUnique(new PassThroughFilter()), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);
  bool error_handler_called1 = false;
  client1.set_connection_error_handler(base::BindOnce(
      &ForwardErrorHandler, &error_handler_called1, run_loop1.QuitClosure()));

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue));
  run_loop3.Run();

  // The request has been received but no response has been sent.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Destroy the responder MessagerReceiver but don't send any response.
  generator.CompleteWithoutResponse();
  run_loop0.Run();
  run_loop1.Run();

  // Check that no response was received.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Connection error handler is called at both sides.
  EXPECT_TRUE(error_handler_called0);
  EXPECT_TRUE(error_handler_called1);

  // The error flag is set at both sides.
  EXPECT_TRUE(client0.encountered_error());
  EXPECT_TRUE(client1.encountered_error());

  // The message pipe handle is valid at both sides.
  EXPECT_TRUE(router0_->is_valid());
  EXPECT_TRUE(router1_->is_valid());
}

TEST_F(MultiplexRouterTest, LateResponse) {
  // Test that things won't blow up if we try to send a message to a
  // MessageReceiver, which was given to us via AcceptWithResponder,
  // after the router has gone away.

  base::RunLoop run_loop;
  LazyResponseGenerator generator(run_loop.QuitClosure());
  {
    MessageQueue message_queue;
    InterfaceEndpointClient client0(
        std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(),
        {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
        kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);
    InterfaceEndpointClient client1(
        std::move(endpoint1_), &generator,
        std::make_unique<PassThroughFilter>(), {},
        base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
        kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

    Message request;
    AllocRequestMessage(1, "hello", &request);
    client0.AcceptWithResponder(
        &request, std::make_unique<MessageAccumulator>(&message_queue));

    run_loop.Run();

    EXPECT_TRUE(generator.has_responder());
  }

  EXPECT_FALSE(generator.responder_is_valid());
  generator.CompleteWithResponse();  // This should end up doing nothing.
}

// TODO(yzshen): add more tests.

// Validates that PassMessagePipe() correctly extracts the underlying pipe,
// leaving the router in a valid-but-disconnected state, and that the
// extracted pipe can be used to establish a new connection.
//
// PassMessagePipe() is used by Unbind() (e.g., mojo::Remote::Unbind()) to
// detach the pipe from one router so it can be re-bound to another. A
// concrete scenario: a service restarts and the caller unbinds the old
// remote, extracts the pipe, and binds it to a new router to reconnect.
//
// API contract: PassMessagePipe() requires that all associated endpoints
// are closed first (DCHECK(!HasAssociatedEndpoints())). After calling it,
// the router is no longer valid (is_valid() returns false) and should only
// hold the test's reference (HasOneRef()).
TEST_F(MultiplexRouterTest, PassMessagePipe) {
  std::unique_ptr<InterfaceEndpointClient> client0 =
      std::make_unique<InterfaceEndpointClient>(
          std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(),
          base::span<const uint32_t>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
          kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  std::unique_ptr<InterfaceEndpointClient> client1 =
      std::make_unique<InterfaceEndpointClient>(
          std::move(endpoint1_), nullptr, std::make_unique<PassThroughFilter>(),
          base::span<const uint32_t>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
          kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  base::RunLoop run_loop0;
  client1->set_connection_error_handler(run_loop0.QuitClosure());

  // At the beginning, both routers are valid and have associated endpoints.
  EXPECT_TRUE(router0_->is_valid());
  EXPECT_TRUE(router0_->HasAssociatedEndpoints());
  EXPECT_TRUE(router1_->is_valid());
  EXPECT_TRUE(router1_->HasAssociatedEndpoints());

  // Next, kill the InterfaceEndpointClient on router0_ side.
  client0.reset();     // This will call DetachClient on router0_
  endpoint0_.reset();  // This will update the endpoint state on router0_ side
                       // to closed.
  // router0_ should have associated endpoints because the endpoint
  // still exists on the other side.
  EXPECT_TRUE(router0_->HasAssociatedEndpoints());
  EXPECT_TRUE(router1_->is_valid());

  // Get the OnPipeConnectionError callback to run for router1_.
  run_loop0.Run();

  // The Peer is closed, but the endpoint should still be associated because
  // Close has not been called.
  EXPECT_TRUE(router1_->is_valid());
  EXPECT_TRUE(router1_->HasAssociatedEndpoints());

  client1.reset();  // This will call DetachClient on router1_
  endpoint1_.reset();

  // Endpoints should be dead in Router 1, but still a good router
  EXPECT_TRUE(router1_->is_valid());
  EXPECT_FALSE(router1_->HasAssociatedEndpoints());

  // Router 0 has not had updates processed yet
  EXPECT_TRUE(router0_->is_valid());
  EXPECT_TRUE(router0_->HasAssociatedEndpoints());

  // Get updates reflected on Router0 side.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !router0_->HasAssociatedEndpoints(); }));
  EXPECT_TRUE(router0_->is_valid());

  auto message_pipe = router0_->PassMessagePipe();
  EXPECT_FALSE(router0_->is_valid());
  EXPECT_FALSE(router0_->HasAssociatedEndpoints());

  // The only reference to router0_ should be the test's at this point.
  EXPECT_TRUE(router0_->HasOneRef());

  // Now hook up a new router2 to prove we can continue using router1_
  scoped_refptr<MultiplexRouter> router2 =
      MultiplexRouter::CreateAndStartReceiving(
          std::move(message_pipe), MultiplexRouter::MULTI_INTERFACE, false,
          base::SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(router2->is_valid());

  ScopedInterfaceEndpointHandle endpoint2;
  ScopedInterfaceEndpointHandle endpoint1_1;
  ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&endpoint2,
                                                              &endpoint1_1);
  auto id2 = router2->AssociateInterface(std::move(endpoint1_1));
  endpoint1_1 = router1_->CreateLocalEndpointHandle(id2);

  EXPECT_TRUE(router2->is_valid());
  EXPECT_TRUE(router2->HasAssociatedEndpoints());
  EXPECT_TRUE(router1_->is_valid());
  EXPECT_TRUE(router1_->HasAssociatedEndpoints());

  InterfaceEndpointClient client2(
      std::move(endpoint2), nullptr, std::make_unique<PassThroughFilter>(), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);

  ResponseGenerator generator;
  std::unique_ptr<InterfaceEndpointClient> client1_new =
      std::make_unique<InterfaceEndpointClient>(
          std::move(endpoint1_1), &generator,
          std::make_unique<PassThroughFilter>(), base::span<const uint32_t>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
          kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  // Send a message on router1_ to verify it's still functional.
  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop;
  client2.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue,
                                                     run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string_view("hello world!"), GetPayloadAsStringView(response));
}

TEST_F(MultiplexRouterTest,
       InterfaceEndpointClientDestroyKillsMultiplexRouter) {
  std::unique_ptr<InterfaceEndpointClient> client0 =
      std::make_unique<InterfaceEndpointClient>(
          std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(),
          base::span<const uint32_t>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
          kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  std::unique_ptr<InterfaceEndpointClient> client1 =
      std::make_unique<InterfaceEndpointClient>(
          std::move(endpoint1_), nullptr, std::make_unique<PassThroughFilter>(),
          base::span<const uint32_t>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
          kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  // Release the Endpoint/InterfaceEndpointClient references, normally these
  // would be the final references keeping the Routers alive.
  client1.reset();
  client0.reset();
  endpoint1_.reset();
  endpoint0_.reset();

  // The test owns these references.
  EXPECT_TRUE(router0_->HasOneRef());
  EXPECT_TRUE(router1_->HasOneRef());
}

// Validates that the posted-Release mechanism correctly handles the race
// between a pipe error callback and off-sequence destruction.
//
// The race: SimpleWatcher detects a pipe error and posts
// Connector::OnWatcherHandleReady to the bound sequence. Concurrently, the
// last scoped_refptr is released off-sequence. Without the fix, the
// Unretained(Connector*) in the watcher callback would dangle.
//
// The fix (AssociatedGroupController::Release) posts off-sequence releases
// to the bound sequence, serializing destruction with the watcher callback.
// This test verifies that OnWatcherHandleReady completes successfully even
// when destruction is racing, and that the router is cleanly destroyed
// afterward.
TEST_F(MultiplexRouterTest, DeathDuringHandleError) {
  std::unique_ptr<RaceConditionScheduler> scheduler =
      std::make_unique<RaceConditionScheduler>();
  EXPECT_FALSE(scheduler->IsRouterDead());

  // The Scheduler should hold the only reference to the router.
  EXPECT_TRUE(scheduler->RouterHasOneRef());

  base::RunLoop error_loop;
  // This will post an asynchronous task to handle the error,
  // mimicking the SimpleWatcher having an error trying to post to
  // the other end of the connection
  scheduler->ScheduleOnWatcherHandleReady(error_loop.QuitClosure(),
                                          MOJO_RESULT_FAILED_PRECONDITION);

  // Schedule the router for death and wait for it to start
  base::RunLoop death_loop;
  scheduler->ScheduleDeathAndWait(death_loop.QuitClosure());

  // We should see the quit closure indicating our scheduled
  // OnWatcherHandleReady has run if Connector did not die first.
  error_loop.Run();

  // Death should be signaled without crashing
  scheduler->SignalDeath();

  death_loop.Run();

  // The router should be gone now
  EXPECT_TRUE(scheduler->IsRouterDead());
}

// Validates that calling NotifyLocalEndpointOfPeerClosure off-sequence while
// the last ref is released doesn't cause use-after-free.
//
// NotifyLocalEndpointOfPeerClosure is safe to call from any sequence — it
// posts to the bound sequence via WrapRefCounted(this). The posted Release
// ensures the ref count is >= 1 when that posted task runs, so the
// WrapRefCounted call is safe. Without the fix, the ref count could reach
// zero before the posted NotifyLocalEndpointOfPeerClosure task runs.
TEST_F(MultiplexRouterTest, NotifyPeerClosureRacesWithDeath) {
  auto scheduler = std::make_unique<RaceConditionScheduler>();
  EXPECT_FALSE(scheduler->IsRouterDead());
  EXPECT_TRUE(scheduler->RouterHasOneRef());

  base::RunLoop error_loop;
  base::RunLoop death_loop;

  // Call NotifyLocalEndpointOfPeerClosure from the racing thread
  // (off-sequence), then release the last ref. The posted Release serializes
  // destruction after any work posted by NotifyLocalEndpointOfPeerClosure.
  scheduler->ScheduleNotifyPeerClosureAndDeath(
      error_loop.QuitClosure(), death_loop.QuitClosure(), kPrimaryInterfaceId);

  error_loop.Run();

  // Signal that destruction can proceed.
  scheduler->SignalDeath();
  death_loop.Run();

  EXPECT_TRUE(scheduler->IsRouterDead());
}

// Validates that calling RaiseError off-sequence while the last ref is
// released doesn't cause use-after-free.
//
// RaiseError is safe to call from any sequence — it posts to the bound
// sequence via WrapRefCounted(this). Same invariant as
// NotifyPeerClosureRacesWithDeath: the posted Release ensures the ref count
// is >= 1 when the RaiseError task runs on the bound sequence.
TEST_F(MultiplexRouterTest, RaiseErrorRacesWithDeath) {
  auto scheduler = std::make_unique<RaceConditionScheduler>();
  EXPECT_FALSE(scheduler->IsRouterDead());
  EXPECT_TRUE(scheduler->RouterHasOneRef());

  base::RunLoop error_loop;
  base::RunLoop death_loop;

  // Call RaiseError from the racing thread (off-sequence), then release the
  // last ref. The posted Release serializes destruction after any work posted
  // by RaiseError.
  scheduler->ScheduleRaiseErrorAndDeath(error_loop.QuitClosure(),
                                        death_loop.QuitClosure());

  error_loop.Run();

  // Signal that destruction can proceed.
  scheduler->SignalDeath();
  death_loop.Run();

  EXPECT_TRUE(scheduler->IsRouterDead());
}

// Validates that the router's destructor safely cleans up queued tasks.
//
// When messages are pending in the router's task queue and the router is
// destroyed (e.g., because all clients disconnected), the destructor must
// clear tasks_ and endpoints_ without crashing. This tests the teardown
// path where MessageWrapper::~MessageWrapper runs during ~MultiplexRouter.
TEST_F(MultiplexRouterTest, PendingMessagesCleanedUpDuringDestruction) {
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);
  base::RunLoop generator_loop;
  LazyResponseGenerator generator(generator_loop.QuitClosure());
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator, std::make_unique<PassThroughFilter>(),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  // Send a message so there's a pending task in the router's queue.
  Message request;
  AllocRequestMessage(1, "hello", &request);
  MessageQueue message_queue;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue));

  // Wait for the request to arrive at client1.
  generator_loop.Run();

  // Don't respond — leave the message pending. Now tear everything down.
  // The router should safely destroy queued MessageWrappers without
  // accessing partially-freed endpoints (guarded by HasAtLeastOneRef).
  generator.CompleteWithoutResponse();

  // Destroy clients and endpoints. This leaves pending tasks in the router.
  // The router's destructor (via TearDown) will clear tasks_ and endpoints_.
  // MessageWrapper::~MessageWrapper must not crash.
}

// Tests that closing the peer pipe triggers the error handler correctly and
// the router remains valid during error processing.
TEST_F(MultiplexRouterTest, ErrorHandlerDuringMessageProcessing) {
  InterfaceEndpointClient client0(
      std::move(endpoint0_), nullptr, std::make_unique<PassThroughFilter>(), {},
      base::SingleThreadTaskRunner::GetCurrentDefault(), 0u, kTestInterfaceName,
      MessageToMethodInfo, MessageToMethodName);
  base::RunLoop generator_loop;
  LazyResponseGenerator generator(generator_loop.QuitClosure());
  InterfaceEndpointClient client1(
      std::move(endpoint1_), &generator, std::make_unique<PassThroughFilter>(),
      {}, base::SingleThreadTaskRunner::GetCurrentDefault(), 0u,
      kTestInterfaceName, MessageToMethodInfo, MessageToMethodName);

  // Send a message, then close the peer before the response arrives.
  Message request;
  AllocRequestMessage(1, "hello", &request);
  MessageQueue message_queue;
  client0.AcceptWithResponder(
      &request, std::make_unique<MessageAccumulator>(&message_queue));

  // Let the request arrive at client1.
  generator_loop.Run();

  // Close the peer side while a response is pending.
  bool error_called = false;
  base::RunLoop run_loop;
  client0.set_connection_error_handler(
      base::BindLambdaForTesting([&error_called, &run_loop] {
        error_called = true;
        run_loop.Quit();
      }));

  generator.CompleteWithResponse();
  router1_->CloseMessagePipe();
  router1_ = nullptr;
  run_loop.Run();

  EXPECT_TRUE(error_called);
}

// Validates the posted-Release mechanism end-to-end: CloseMessagePipe is
// called on the bound sequence (as required by the API contract), then the
// last scoped_refptr is released from a different thread.
//
// AssociatedGroupController::Release() detects the off-sequence release and
// posts the ref-count decrement back to the bound sequence. This test
// verifies that the destructor runs on the bound sequence without crashing,
// even though the final Release originated from a different thread.
TEST_F(MultiplexRouterTest, CloseAndDestroyFromDifferentThread) {
  base::Thread other_thread("DestroyThread");
  ASSERT_TRUE(other_thread.Start());

  endpoint1_.reset();
  endpoint0_.reset();

  // Close the pipes on the bound sequence (required by API contract).
  router1_->CloseMessagePipe();
  router0_->CloseMessagePipe();

  // Move routers to the other thread and release them there.
  scoped_refptr<MultiplexRouter> router0 = std::move(router0_);
  scoped_refptr<MultiplexRouter> router1 = std::move(router1_);

  // Releasing on the other thread triggers the posted-Release mechanism,
  // which posts the ref-count decrement back to the main thread. After
  // releasing, post a quit closure to the main thread so we can pump until
  // those deletions complete.
  base::RunLoop run_loop;
  auto main_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto quit = run_loop.QuitClosure();
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [r0 = std::move(router0), r1 = std::move(router1),
                      main_runner, quit = std::move(quit)]() mutable {
                       r1 = nullptr;
                       r0 = nullptr;
                       // Posted after the Release tasks, so the deletions
                       // will be processed before the loop quits.
                       main_runner->PostTask(FROM_HERE, std::move(quit));
                     }));
  run_loop.Run();
}

}  // namespace
}  // namespace test
}  // namespace mojo
