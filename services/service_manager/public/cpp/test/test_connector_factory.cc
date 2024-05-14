// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_connector_factory.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

namespace {

class ProxiedServiceConnector : public mojom::Connector {
 public:
  ProxiedServiceConnector(
      TestConnectorFactory* factory,
      TestConnectorFactory::NameToServiceProxyMap* proxies,
      const base::Token& test_instance_group)
      : fake_guid_(base::Token::CreateRandom()),
        factory_(factory),
        proxies_(proxies),
        test_instance_group_(test_instance_group) {}

  ProxiedServiceConnector(const ProxiedServiceConnector&) = delete;
  ProxiedServiceConnector& operator=(const ProxiedServiceConnector&) = delete;

  ~ProxiedServiceConnector() override = default;

 private:
  mojom::Service* GetServiceProxy(const std::string& service_name) {
    auto proxy_it = proxies_->find(service_name);
    if (proxy_it != proxies_->end())
      return proxy_it->second.get();

    return nullptr;
  }

  // mojom::Connector:
  void BindInterface(const ServiceFilter& service_filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     mojom::BindInterfacePriority priority,
                     BindInterfaceCallback callback) override {
    auto* proxy = GetServiceProxy(service_filter.service_name());
    if (!proxy && factory_->ignore_unknown_service_requests()) {
      std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED,
                              std::nullopt);
      return;
    }

    CHECK(proxy)
        << "TestConnectorFactory received a BindInterface request for an "
        << "unregistered service '" << service_filter.service_name() << "'";
    proxy->OnBindInterface(
        BindSourceInfo(Identity("TestConnectorFactory", test_instance_group_,
                                base::Token{}, fake_guid_),
                       CapabilitySet()),
        interface_name, std::move(interface_pipe), base::DoNothing());
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, std::nullopt);
  }

  void WarmService(const ServiceFilter& filter,
                   WarmServiceCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void QueryService(const std::string& service_name,
                    QueryServiceCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void RegisterServiceInstance(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
      RegisterServiceInstanceCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void Clone(mojo::PendingReceiver<mojom::Connector> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  const base::Token fake_guid_;
  const raw_ptr<TestConnectorFactory> factory_;
  const raw_ptr<TestConnectorFactory::NameToServiceProxyMap> proxies_;
  const base::Token test_instance_group_;
  mojo::ReceiverSet<mojom::Connector> receivers_;
};

}  // namespace

TestConnectorFactory::TestConnectorFactory() {
  test_instance_group_ = base::Token::CreateRandom();
  impl_ = std::make_unique<ProxiedServiceConnector>(this, &service_proxies_,
                                                    test_instance_group_);
}

TestConnectorFactory::~TestConnectorFactory() = default;

Connector* TestConnectorFactory::GetDefaultConnector() {
  if (!default_connector_)
    default_connector_ = CreateConnector();
  return default_connector_.get();
}

std::unique_ptr<Connector> TestConnectorFactory::CreateConnector() {
  mojo::PendingRemote<mojom::Connector> proxy;
  impl_->Clone(proxy.InitWithNewPipeAndPassReceiver());
  return std::make_unique<Connector>(std::move(proxy));
}

mojo::PendingReceiver<mojom::Service> TestConnectorFactory::RegisterInstance(
    const std::string& service_name) {
  mojo::Remote<mojom::Service> proxy_remote;
  mojo::PendingReceiver<mojom::Service> receiver =
      proxy_remote.BindNewPipeAndPassReceiver();
  proxy_remote->OnStart(
      Identity(service_name, test_instance_group_, base::Token{},
               base::Token::CreateRandom()),
      base::BindOnce(&TestConnectorFactory::OnStartResponseHandler,
                     base::Unretained(this), service_name));
  service_proxies_[service_name] = std::move(proxy_remote);
  return receiver;
}

void TestConnectorFactory::OnStartResponseHandler(
    const std::string& service_name,
    mojo::PendingReceiver<mojom::Connector> connector_receiver,
    mojo::PendingAssociatedReceiver<mojom::ServiceControl> control_receiver) {
  impl_->Clone(std::move(connector_receiver));
  service_control_receivers_.Add(this, std::move(control_receiver),
                                 service_name);
}

void TestConnectorFactory::RequestQuit() {
  if (ignore_quit_requests_)
    return;

  service_proxies_.erase(service_control_receivers_.current_context());
  service_control_receivers_.Remove(
      service_control_receivers_.current_receiver());
}

}  // namespace service_manager
