// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

class ReceiverSetStaticAssertTests {
  // The receiver entry in a receiver set with no context should be the same
  // size as a regular receiver + one machine word for the vtable.
  static_assert(sizeof(Receiver<PingService>) + sizeof(void*) ==
                sizeof(ReceiverSet<PingService>::ReceiverEntry));
};

namespace {

using ReceiverSetTest = BindingsTestBase;

template <typename ReceiverSetType, typename ContextType>
void ExpectContextHelper(const ReceiverSetType* receiver_set,
                         ContextType expected_context) {
  EXPECT_EQ(expected_context, receiver_set->current_context());
}

template <typename ReceiverSetType, typename ContextType>
base::RepeatingClosure ExpectContext(const ReceiverSetType* receiver_set,
                                     ContextType expected_context) {
  return base::BindRepeating(&ExpectContextHelper<ReceiverSetType, ContextType>,
                             receiver_set, expected_context);
}

template <typename ReceiverSetType, typename ContextType>
void ExpectMutableContextHelper(ReceiverSetType* receiver_set,
                                ContextType expected_context,
                                ContextType new_context) {
  {
    ContextType& context = receiver_set->current_context();
    EXPECT_EQ(context, expected_context);
    context = new_context;
  }

  ExpectContextHelper(receiver_set, new_context);
}

template <typename ReceiverSetType, typename ContextType>
base::RepeatingClosure ExpectMutableContext(ReceiverSetType* receiver_set,
                                            ContextType expected_context,
                                            ContextType new_context) {
  return base::BindRepeating(
      &ExpectMutableContextHelper<ReceiverSetType, ContextType>, receiver_set,
      expected_context, new_context);
}

template <typename ReceiverSetType>
void ExpectReceiverIdHelper(ReceiverSetType* receiver_set,
                            ReceiverId expected_receiver_id) {
  EXPECT_EQ(expected_receiver_id, receiver_set->current_receiver());
}

template <typename ReceiverSetType>
base::RepeatingClosure ExpectReceiverId(ReceiverSetType* receiver_set,
                                        ReceiverId expected_receiver_id) {
  return base::BindRepeating(&ExpectReceiverIdHelper<ReceiverSetType>,
                             receiver_set, expected_receiver_id);
}

template <typename ReceiverSetType>
void ReportBadMessageHelper(ReceiverSetType* receiver_set,
                            const std::string& error) {
  receiver_set->ReportBadMessage(error);
}

template <typename ReceiverSetType>
base::RepeatingClosure ReportBadMessage(ReceiverSetType* receiver_set,
                                        const std::string& error) {
  return base::BindRepeating(&ReportBadMessageHelper<ReceiverSetType>,
                             receiver_set, error);
}

template <typename ReceiverSetType>
void SaveBadMessageCallbackHelper(ReceiverSetType* receiver_set,
                                  ReportBadMessageCallback* callback) {
  *callback = receiver_set->GetBadMessageCallback();
}

template <typename ReceiverSetType>
base::RepeatingClosure SaveBadMessageCallback(
    ReceiverSetType* receiver_set,
    ReportBadMessageCallback* callback) {
  return base::BindRepeating(&SaveBadMessageCallbackHelper<ReceiverSetType>,
                             receiver_set, callback);
}

base::RepeatingClosure Sequence(base::RepeatingClosure first,
                                base::RepeatingClosure second) {
  return base::BindRepeating(
      [](base::RepeatingClosure first, base::RepeatingClosure second) {
        first.Run();
        second.Run();
      },
      first, second);
}

class PingImpl : public PingService {
 public:
  PingImpl() = default;
  ~PingImpl() override = default;

  void set_ping_handler(const base::RepeatingClosure& handler) {
    ping_handler_ = handler;
  }

 private:
  // PingService:
  void Ping(PingCallback callback) override {
    if (!ping_handler_.is_null())
      ping_handler_.Run();
    std::move(callback).Run();
  }

  base::RepeatingClosure ping_handler_;
};

TEST_P(ReceiverSetTest, ReceiverSetContext) {
  PingImpl impl;

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  {
    impl.set_ping_handler(ExpectContext(&receivers, 1));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&receivers, 2));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectContext(&receivers, 1), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectContext(&receivers, 2), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(receivers.empty());
}

TEST_P(ReceiverSetTest, ReceiverSetMutableContext) {
  PingImpl impl;

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  {
    impl.set_ping_handler(ExpectMutableContext(&receivers, 1, 3));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectMutableContext(&receivers, 2, 4));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }
}

TEST_P(ReceiverSetTest, ReceiverSetGetContext) {
  PingImpl impl;

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  ReceiverId receiver_a =
      receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  ReceiverId receiver_b =
      receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  EXPECT_EQ(1, *receivers.GetContext(receiver_a));
  EXPECT_EQ(2, *receivers.GetContext(receiver_b));
  EXPECT_EQ(nullptr, receivers.GetContext(receiver_b + 1));
}

TEST_P(ReceiverSetTest, ReceiverSetGetAllContexts) {
  PingImpl impl;

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  ReceiverId receiver_a =
      receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  ReceiverId receiver_b =
      receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  std::map<ReceiverId, int*> contexts = receivers.GetAllContexts();
  EXPECT_EQ(2u, contexts.size());
  EXPECT_EQ(1, *contexts[receiver_a]);
  EXPECT_EQ(2, *contexts[receiver_b]);
}

TEST_P(ReceiverSetTest, ReceiverSetDispatchReceiver) {
  PingImpl impl;

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  ReceiverId id_a =
      receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  ReceiverId id_b =
      receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  {
    impl.set_ping_handler(ExpectReceiverId(&receivers, id_a));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectReceiverId(&receivers, id_b));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectReceiverId(&receivers, id_a), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectReceiverId(&receivers, id_b), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(receivers.empty());
}

TEST_P(ReceiverSetTest, ReceiverSetConnectionErrorWithReason) {
  PingImpl impl;
  Remote<PingService> remote;
  ReceiverSet<PingService> receivers;
  receivers.Add(&impl, remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  receivers.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(1024u, custom_reason);
        EXPECT_EQ("bye", description);
        run_loop.Quit();
      }));

  remote.ResetWithReason(1024u, "bye");
}

TEST_P(ReceiverSetTest, ReceiverSetReportBadMessage) {
  PingImpl impl;

  std::string last_received_error;
  SetDefaultProcessErrorHandler(
      base::BindRepeating([](std::string* out_error,
                             const std::string& error) { *out_error = error; },
                          &last_received_error));

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  {
    impl.set_ping_handler(ReportBadMessage(&receivers, "message 1"));
    base::RunLoop loop;
    ping_a.set_disconnect_handler(loop.QuitClosure());
    ping_a->Ping(base::BindOnce([] {}));
    loop.Run();
    EXPECT_EQ("message 1", last_received_error);
  }

  {
    impl.set_ping_handler(ReportBadMessage(&receivers, "message 2"));
    base::RunLoop loop;
    ping_b.set_disconnect_handler(loop.QuitClosure());
    ping_b->Ping(base::BindOnce([] {}));
    loop.Run();
    EXPECT_EQ("message 2", last_received_error);
  }

  EXPECT_TRUE(receivers.empty());

  SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_P(ReceiverSetTest, ReceiverSetGetBadMessageCallback) {
  PingImpl impl;

  std::string last_received_error;
  SetDefaultProcessErrorHandler(
      base::BindRepeating([](std::string* out_error,
                             const std::string& error) { *out_error = error; },
                          &last_received_error));

  ReceiverSet<PingService, int> receivers;
  Remote<PingService> ping_a, ping_b;
  receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);
  receivers.Add(&impl, ping_b.BindNewPipeAndPassReceiver(), 2);

  ReportBadMessageCallback bad_message_callback_a;
  ReportBadMessageCallback bad_message_callback_b;

  {
    impl.set_ping_handler(
        SaveBadMessageCallback(&receivers, &bad_message_callback_a));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
    ping_a.reset();
  }

  {
    impl.set_ping_handler(
        SaveBadMessageCallback(&receivers, &bad_message_callback_b));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  std::move(bad_message_callback_a).Run("message 1");
  EXPECT_EQ("message 1", last_received_error);

  {
    base::RunLoop loop;
    ping_b.set_disconnect_handler(loop.QuitClosure());
    std::move(bad_message_callback_b).Run("message 2");
    EXPECT_EQ("message 2", last_received_error);
    loop.Run();
  }

  EXPECT_TRUE(receivers.empty());

  SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_P(ReceiverSetTest, ReceiverSetGetBadMessageCallbackOutlivesReceiverSet) {
  PingImpl impl;

  std::string last_received_error;
  SetDefaultProcessErrorHandler(
      base::BindRepeating([](std::string* out_error,
                             const std::string& error) { *out_error = error; },
                          &last_received_error));

  ReportBadMessageCallback bad_message_callback;
  {
    ReceiverSet<PingService, int> receivers;
    Remote<PingService> ping_a;
    receivers.Add(&impl, ping_a.BindNewPipeAndPassReceiver(), 1);

    impl.set_ping_handler(
        SaveBadMessageCallback(&receivers, &bad_message_callback));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  std::move(bad_message_callback).Run("message 1");
  EXPECT_EQ("message 1", last_received_error);

  SetDefaultProcessErrorHandler(base::NullCallback());
}

class PingProviderImpl : public AssociatedPingProvider, public PingService {
 public:
  PingProviderImpl() = default;
  ~PingProviderImpl() override = default;

  void set_new_ping_context(int context) { new_ping_context_ = context; }

  void set_new_ping_handler(base::RepeatingClosure handler) {
    new_ping_handler_ = std::move(handler);
  }

  void set_ping_handler(base::RepeatingClosure handler) {
    ping_handler_ = std::move(handler);
  }

  AssociatedReceiverSet<PingService, int>& ping_receivers() {
    return ping_receivers_;
  }

 private:
  // AssociatedPingProvider:
  void GetPing(PendingAssociatedReceiver<PingService> receiver) override {
    ping_receivers_.Add(this, std::move(receiver), new_ping_context_);
    if (!new_ping_handler_.is_null())
      new_ping_handler_.Run();
  }

  // PingService:
  void Ping(PingCallback callback) override {
    if (!ping_handler_.is_null())
      ping_handler_.Run();
    std::move(callback).Run();
  }

  AssociatedReceiverSet<PingService, int> ping_receivers_;
  int new_ping_context_ = -1;
  base::RepeatingClosure ping_handler_;
  base::RepeatingClosure new_ping_handler_;
};

TEST_P(ReceiverSetTest, AssociatedReceiverSetContext) {
  Remote<AssociatedPingProvider> provider;
  PingProviderImpl impl;
  Receiver<AssociatedPingProvider> receiver(
      &impl, provider.BindNewPipeAndPassReceiver());

  AssociatedRemote<PingService> ping_a;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(1);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(ping_a.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  AssociatedRemote<PingService> ping_b;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(2);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(ping_b.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_receivers(), 1));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_receivers(), 2));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_receivers().set_disconnect_handler(
        Sequence(ExpectContext(&impl.ping_receivers(), 1), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_receivers().set_disconnect_handler(
        Sequence(ExpectContext(&impl.ping_receivers(), 2), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(impl.ping_receivers().empty());
}

TEST_P(ReceiverSetTest, PrimaryInterfaceReceiverSetContext) {
  Remote<AssociatedPingProvider> provider_a, provider_b;
  PingProviderImpl impl;
  ReceiverSet<AssociatedPingProvider, int> receivers;

  receivers.Add(&impl, provider_a.BindNewPipeAndPassReceiver(), 1);
  receivers.Add(&impl, provider_b.BindNewPipeAndPassReceiver(), 2);

  {
    AssociatedRemote<PingService> ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&receivers, 1), loop.QuitClosure()));
    provider_a->GetPing(ping.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  {
    AssociatedRemote<PingService> ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&receivers, 2), loop.QuitClosure()));
    provider_b->GetPing(ping.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectContext(&receivers, 1), loop.QuitClosure()));
    provider_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectContext(&receivers, 2), loop.QuitClosure()));
    provider_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(receivers.empty());
}

TEST_P(ReceiverSetTest, PrimaryInterfaceReceiverSetDispatchReceiver) {
  Remote<AssociatedPingProvider> provider_a, provider_b;
  PingProviderImpl impl;
  ReceiverSet<AssociatedPingProvider, int> receivers;

  ReceiverId id_a =
      receivers.Add(&impl, provider_a.BindNewPipeAndPassReceiver(), 1);
  ReceiverId id_b =
      receivers.Add(&impl, provider_b.BindNewPipeAndPassReceiver(), 2);

  {
    AssociatedRemote<PingService> ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectReceiverId(&receivers, id_a), loop.QuitClosure()));
    provider_a->GetPing(ping.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  {
    AssociatedRemote<PingService> ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectReceiverId(&receivers, id_b), loop.QuitClosure()));
    provider_b->GetPing(ping.BindNewEndpointAndPassReceiver());
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectReceiverId(&receivers, id_a), loop.QuitClosure()));
    provider_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    receivers.set_disconnect_handler(
        Sequence(ExpectReceiverId(&receivers, id_b), loop.QuitClosure()));
    provider_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(receivers.empty());
}

TEST_P(ReceiverSetTest, AssociatedReceiverSetConnectionErrorWithReason) {
  Remote<AssociatedPingProvider> remote_primary;
  PingProviderImpl primary_impl;
  Receiver<AssociatedPingProvider> primary_receiver(
      &primary_impl, remote_primary.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  primary_impl.ping_receivers().set_disconnect_with_reason_handler(
      base::BindLambdaForTesting(
          [&](uint32_t custom_reason, const std::string& description) {
            EXPECT_EQ(2048u, custom_reason);
            EXPECT_EQ("bye", description);
            run_loop.Quit();
          }));

  AssociatedRemote<PingService> remote_ping;
  remote_primary->GetPing(remote_ping.BindNewEndpointAndPassReceiver());

  remote_ping.ResetWithReason(2048u, "bye");

  run_loop.Run();
}

class PingInstanceCounter : public PingService {
 public:
  PingInstanceCounter() { ++instance_count; }
  ~PingInstanceCounter() override { --instance_count; }

  void Ping(PingCallback callback) override {}

  static int instance_count;
};
int PingInstanceCounter::instance_count = 0;

TEST_P(ReceiverSetTest, UniqueReceiverSetDestruction) {
  Remote<PingService> ping_a, ping_b;
  auto receivers = std::make_unique<UniqueReceiverSet<PingService>>();

  receivers->Add(std::make_unique<PingInstanceCounter>(),
                 ping_a.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  receivers->Add(std::make_unique<PingInstanceCounter>(),
                 ping_b.BindNewPipeAndPassReceiver());
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  receivers.reset();
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

TEST_P(ReceiverSetTest, UniqueReceiverSetDisconnect) {
  Remote<PingService> ping_a, ping_b;
  UniqueReceiverSet<PingService> receivers;
  receivers.Add(std::make_unique<PingInstanceCounter>(),
                ping_a.BindNewPipeAndPassReceiver());
  receivers.Add(std::make_unique<PingInstanceCounter>(),
                ping_b.BindNewPipeAndPassReceiver());
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  ping_a.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  ping_b.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

TEST_P(ReceiverSetTest, UniqueReceiverSetRemoveEntry) {
  Remote<PingService> ping_a, ping_b;
  UniqueReceiverSet<PingService> receivers;
  ReceiverId receiver_id_a =
      receivers.Add(std::make_unique<PingInstanceCounter>(),
                    ping_a.BindNewPipeAndPassReceiver());
  ReceiverId receiver_id_b =
      receivers.Add(std::make_unique<PingInstanceCounter>(),
                    ping_b.BindNewPipeAndPassReceiver());
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  EXPECT_TRUE(receivers.Remove(receiver_id_a));
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  EXPECT_TRUE(receivers.Remove(receiver_id_b));
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ReceiverSetTest);

}  // namespace
}  // namespace test
}  // namespace mojo
