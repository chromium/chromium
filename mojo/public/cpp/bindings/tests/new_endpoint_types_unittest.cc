// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/interfaces/bindings/tests/new_endpoint_types.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace new_endpoint_types {

class FactoryImpl;

class WidgetImpl : public mojom::Widget {
 public:
  explicit WidgetImpl(mojo::PendingRemote<mojom::WidgetClient> client)
      : client_(std::move(client)) {
    client_->OnInitialized();
  }

  WidgetImpl(const WidgetImpl&) = delete;
  WidgetImpl& operator=(const WidgetImpl&) = delete;

  ~WidgetImpl() override = default;

  // mojom::Widget:
  void Click() override {
    for (auto& observer : observers_)
      observer->OnClick();
  }

  void AddObserver(
      mojo::PendingRemote<mojom::WidgetObserver> observer) override {
    observers_.emplace_back(std::move(observer));
  }

 private:
  mojo::Remote<mojom::WidgetClient> client_;
  std::vector<mojo::Remote<mojom::WidgetObserver>> observers_;
};

class FactoryImpl : public mojom::WidgetFactory {
 public:
  explicit FactoryImpl(mojo::PendingReceiver<mojom::WidgetFactory> receiver)
      : receiver_(this, std::move(receiver)) {}

  FactoryImpl(const FactoryImpl&) = delete;
  FactoryImpl& operator=(const FactoryImpl&) = delete;

  ~FactoryImpl() override = default;

  // mojom::WidgetFactory:
  void CreateWidget(mojo::PendingReceiver<mojom::Widget> receiver,
                    mojo::PendingRemote<mojom::WidgetClient> client) override {
    widgets_.Add(std::make_unique<WidgetImpl>(std::move(client)),
                 std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::WidgetFactory> receiver_;
  mojo::UniqueReceiverSet<mojom::Widget> widgets_;
};

class ClientImpl : public mojom::WidgetClient {
 public:
  ClientImpl() = default;

  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  ~ClientImpl() override = default;

  mojo::PendingRemote<mojom::WidgetClient> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForInitialize() { wait_loop_.Run(); }

  // mojom::WidgetClient:
  void OnInitialized() override { wait_loop_.Quit(); }

 private:
  mojo::Receiver<mojom::WidgetClient> receiver_{this};
  base::RunLoop wait_loop_;
};

class ObserverImpl : public mojom::WidgetObserver {
 public:
  ObserverImpl() = default;

  ObserverImpl(const ObserverImpl&) = delete;
  ObserverImpl& operator=(const ObserverImpl&) = delete;

  ~ObserverImpl() override = default;

  mojo::PendingRemote<mojom::WidgetObserver> BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&ObserverImpl::OnDisconnect, base::Unretained(this)));
    return remote;
  }

  void WaitForClick() { click_loop_.Run(); }
  void WaitForDisconnect() { disconnect_loop_.Run(); }

  // mojom::WidgetObserver:
  void OnClick() override { click_loop_.Quit(); }

 private:
  void OnDisconnect() { disconnect_loop_.Quit(); }

  mojo::Receiver<mojom::WidgetObserver> receiver_{this};
  base::RunLoop click_loop_;
  base::RunLoop disconnect_loop_;
};

class PingerImpl : public mojom::Pinger {
 public:
  PingerImpl() = default;

  PingerImpl(const PingerImpl&) = delete;
  PingerImpl& operator=(const PingerImpl&) = delete;

  ~PingerImpl() override = default;

  int ping_count() const { return ping_count_; }

  void AddReceiver(mojo::PendingAssociatedReceiver<mojom::Pinger> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  // mojom::Ping:
  void Ping(PingCallback callback) override {
    ++ping_count_;
    std::move(callback).Run();
  }

  mojo::AssociatedReceiverSet<mojom::Pinger> receivers_;
  int ping_count_ = 0;
};

class AssociatedPingerHostImpl : public mojom::AssociatedPingerHost {
 public:
  explicit AssociatedPingerHostImpl(
      mojo::PendingReceiver<mojom::AssociatedPingerHost> receiver)
      : receiver_(this, std::move(receiver)) {}

  AssociatedPingerHostImpl(const AssociatedPingerHostImpl&) = delete;
  AssociatedPingerHostImpl& operator=(const AssociatedPingerHostImpl&) = delete;

  ~AssociatedPingerHostImpl() override = default;

  int ping_count() const { return pinger_.ping_count(); }

 private:
  // mojom::AssociatedPingerHost:
  void AddEndpoints(
      mojo::PendingAssociatedReceiver<mojom::Pinger> receiver,
      mojo::PendingAssociatedRemote<mojom::Pinger> remote) override {
    mojo::AssociatedRemote<mojom::Pinger> pinger(std::move(remote));
    pinger->Ping(base::DoNothing());
    pinger_.AddReceiver(std::move(receiver));
  }

  mojo::Receiver<mojom::AssociatedPingerHost> receiver_;
  PingerImpl pinger_;
};

TEST(NewEndpointTypesTest, BasicUsage) {
  // A simple smoke/compile test for new bindings endpoint types. Used to
  // demonstrate look & feel as well as to ensure basic completeness and
  // correctness.

  base::test::TaskEnvironment task_environment;

  // A Remote<T> exposes a callable T interface which sends messages to a remote
  // implementation of T. Here we create a new unbound Remote which will control
  // a remote implementation of |mojom::WidgetFactory|.
  mojo::Remote<mojom::WidgetFactory> factory;
  EXPECT_FALSE(factory.is_bound());

  // |factory_impl| is a concrete implementation of |mojom::WidgetFactory|. With
  // Mojo interfaces, the implementation can live in the same process as the
  // Remote<T> calling it, or it can live in another process. For simplicity in
  // this test we have the implementation living in the test process.
  //
  // |BindNewPipeAndPassReceiver()| creates a new message pipe to carry
  // |mojom:WidgetFactory| interface messages. It binds one end to the
  // |factory| Remote above, and the other end is passed to |factory_impl| so
  // it can receive messages.
  FactoryImpl factory_impl(factory.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(factory.is_bound());

  // Similar to above, we create another Remote. this time to control a
  // |mojom::Widget| implementation somewhere.
  mojo::Remote<mojom::Widget> widget;

  // |client| is an implementation of |mojom::WidgetClient|. This is a common
  // pattern for Mojo interfaces -- to have a Remote for some Foo interface
  // living alongside a corresponding implementation of a FooClient interface.
  // The pattern allows for two-way communication using separate but
  // closely-related types of endpoints.
  ClientImpl client;

  // Here we send two message pipes to the remote factory. This |CreateWidget|
  // call will be dispatched asynchronously to |factory_impl| via Mojo. Notice
  // that, inline, we create a new |mojom::Widget| pipe as well as a new
  // |mojom::WidgetClient| pipe. The Widget's Receiver endpoint is passed to
  // the factory implementation, as is the WidgetClient's Remote endpoint.
  // This allows the factory to bind and begin receiving Widget messages on
  // one pipe, and to bind and begin sending WidgetClient messages on the other.
  factory->CreateWidget(widget.BindNewPipeAndPassReceiver(),
                        client.BindNewPipeAndPassRemote());

  // Similar to |client| above, we create some implementations of
  // |mojom::WidgetObserver| here to receive messages from Remote
  // WidgetObserver caller on the factory implementation's side of the world.
  ObserverImpl observer1, observer2;

  // Similar to the |CreateWidget| call above, here we create new WidgetObserver
  // pipes (one for each impl object) and pass their Remote ends to the remote
  // Widget implementation to bind and use. This allows the remote Widget
  // implementation to send messages to both |observer1| and |observer2|.
  widget->AddObserver(observer1.BindNewPipeAndPassRemote());
  widget->AddObserver(observer2.BindNewPipeAndPassRemote());

  // When the FactoryImpl asynchronously receives our |CreateWidget| call, it
  // will send back a |mojom::WidgetClient::Initialize()| message to our
  // |client| object using the Remote passed to |CreateWidget|. This waits for
  // that message.
  client.WaitForInitialize();

  // Send another message, this time to the remote Widget implementation.
  widget->Click();

  // When the remote Widget implementation receives a |Click()| message, it
  // broadcasts a |mojom::WidgetObserver::OnClick()| event to all registered
  // WidgetObservers on the Widget. We wait for each of our observers to
  // receive that message here.
  observer1.WaitForClick();
  observer2.WaitForClick();

  // Remotes (and Receivers, for that matter) remain bound until explicitly
  // unbound by their owner.
  widget.reset();
  EXPECT_FALSE(widget.is_bound());

  // Resetting the Remote<Widget> above eventually triggers the remote Widget
  // implementation's disconnection handler. That handler in turn tears down
  // the Widget implementation, including the Remote<WidgetObserver> endpoints
  // it owns. This in turn will eventually trigger our local WidgetObserver
  // instances' disconnection handlers. We wait for that to happen here.
  observer1.WaitForDisconnect();
  observer2.WaitForDisconnect();
}

TEST(NewEndpointTypesTest, AssociatedTypes) {
  base::test::TaskEnvironment task_environment;

  mojo::Remote<mojom::AssociatedPingerHost> host;
  AssociatedPingerHostImpl host_impl(host.BindNewPipeAndPassReceiver());

  PingerImpl test_pinger_impl;
  mojo::PendingAssociatedRemote<mojom::Pinger> test_pinger1;
  mojo::PendingAssociatedRemote<mojom::Pinger> test_pinger2;
  test_pinger_impl.AddReceiver(
      test_pinger1.InitWithNewEndpointAndPassReceiver());
  test_pinger_impl.AddReceiver(
      test_pinger2.InitWithNewEndpointAndPassReceiver());

  mojo::AssociatedRemote<mojom::Pinger> host_pinger1;
  mojo::AssociatedRemote<mojom::Pinger> host_pinger2;

  // Both of these calls should result in a single ping each to |pinger_impl|.
  host->AddEndpoints(host_pinger1.BindNewEndpointAndPassReceiver(),
                     std::move(test_pinger1));
  host->AddEndpoints(host_pinger2.BindNewEndpointAndPassReceiver(),
                     std::move(test_pinger2));

  // Ping each host pinger twice, should result in a total of 4 pings to
  // |host|'s PingerImpl.
  host_pinger1->Ping(base::DoNothing());
  host_pinger1->Ping(base::DoNothing());
  host_pinger2->Ping(base::DoNothing());
  host_pinger2->Ping(base::DoNothing());

  // Should be sufficient to flush all interesting operations, since they all
  // run on the same pipe.
  host.FlushForTesting();

  EXPECT_EQ(4, host_impl.ping_count());
  EXPECT_EQ(2, test_pinger_impl.ping_count());
}

}  // namespace new_endpoint_types
}  // namespace test
}  // namespace mojo
