// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "mojo/public/cpp/bindings/tests/router_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kTestInterfaceName[] = "TestInterface";

IPCStableHashFunction MessageToMethodInfo(Message& message) {
  return nullptr;
}

const char* MessageToMethodName(Message& message) {
  return "method";
}

using mojo::internal::MultiplexRouter;

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

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }

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

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

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

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
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

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  client0.AcceptWithResponder(
      &request2, std::make_unique<MessageAccumulator>(&message_queue));

  router1_->WaitForIncomingMessage();
  router0_->WaitForIncomingMessage();

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
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
  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

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
  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
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

}  // namespace
}  // namespace test
}  // namespace mojo
