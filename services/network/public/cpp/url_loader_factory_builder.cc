// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_factory_builder.h"

namespace network {

URLLoaderFactoryBuilder::URLLoaderFactoryBuilder() = default;
URLLoaderFactoryBuilder::~URLLoaderFactoryBuilder() = default;
URLLoaderFactoryBuilder::URLLoaderFactoryBuilder(URLLoaderFactoryBuilder&&) =
    default;
URLLoaderFactoryBuilder& URLLoaderFactoryBuilder::operator=(
    URLLoaderFactoryBuilder&&) = default;

std::tuple<mojo::PendingReceiver<mojom::URLLoaderFactory>,
           mojo::PendingRemote<mojom::URLLoaderFactory>>
URLLoaderFactoryBuilder::Append() {
  if (IsEmpty()) {
    mojo::PendingRemote<mojom::URLLoaderFactory> tail_factory_remote;
    tail_ = tail_factory_remote.InitWithNewPipeAndPassReceiver();
    return std::make_tuple(head_.InitWithNewPipeAndPassReceiver(),
                           std::move(tail_factory_remote));
  }

  mojo::PendingRemote<mojom::URLLoaderFactory> factory_remote;
  auto factory_receiver = std::move(tail_);
  tail_ = factory_remote.InitWithNewPipeAndPassReceiver();

  // The chain after this call will be:
  //     (chain before this call)-> `factory_receiver`
  //     -(to be bound and connected by the caller)-> `factory_remote`
  //     -(new mojo pipe)-> `tail_`.
  return std::make_tuple(std::move(factory_receiver),
                         std::move(factory_remote));
}

bool URLLoaderFactoryBuilder::IsEmpty() const {
  CHECK_EQ(head_.is_valid(), tail_.is_valid());
  return !head_.is_valid();
}

// We use `Clone()` instead of `mojo::FusePipes()` in the following two methods
// because theoretically the `URLLoaderFactoryBuilder` interface can't ensure
// that `terminal_factory` is fuseable.
//
// TODO(crbug.com/40947547): Consider removing the `Clone()` calls for
// performance. Probably the `terminal_factory` is actually fusable for the
// actual `URLLoaderFactoryBuilder` callers reaching here (e.g.
// `non_network_factories` in `RenderFrameHostImpl::CommitNavigation()` and
// `NavigationURLLoaderImpl::non_network_url_loader_factories_`). Probably we
// should refactor the `non_network_factories` patterns altogether.
void URLLoaderFactoryBuilder::ConnectTerminal(
    mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
    mojo::Remote<mojom::URLLoaderFactory> terminal_factory) {
  terminal_factory->Clone(std::move(pending_receiver));
}

void URLLoaderFactoryBuilder::ConnectTerminal(
    mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
    mojo::PendingRemote<mojom::URLLoaderFactory> terminal_factory) {
  mojo::Remote<mojom::URLLoaderFactory>(std::move(terminal_factory))
      ->Clone(std::move(pending_receiver));
}

void URLLoaderFactoryBuilder::ConnectTerminal(
    mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
    scoped_refptr<SharedURLLoaderFactory> terminal_factory) {
  terminal_factory->Clone(std::move(pending_receiver));
}

void URLLoaderFactoryBuilder::ConnectTerminal(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    network::mojom::NetworkContext* terminal_context,
    network::mojom::URLLoaderFactoryParamsPtr factory_param) {
  terminal_context->CreateURLLoaderFactory(std::move(pending_receiver),
                                           std::move(factory_param));
}

template <>
scoped_refptr<SharedURLLoaderFactory> URLLoaderFactoryBuilder::WrapAs(
    mojo::PendingRemote<mojom::URLLoaderFactory> in) {
  return base::MakeRefCounted<WrapperSharedURLLoaderFactory>(std::move(in));
}

template <>
mojo::PendingRemote<mojom::URLLoaderFactory> URLLoaderFactoryBuilder::WrapAs(
    scoped_refptr<SharedURLLoaderFactory> in) {
  mojo::PendingRemote<mojom::URLLoaderFactory> remote;
  in->Clone(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

PendingSharedURLLoaderFactoryWithBuilder::
    PendingSharedURLLoaderFactoryWithBuilder(
        URLLoaderFactoryBuilder factory_builder,
        std::unique_ptr<PendingSharedURLLoaderFactory> terminal_pending_factory)
    : factory_builder_(std::move(factory_builder)),
      terminal_pending_factory_(std::move(terminal_pending_factory)) {}
PendingSharedURLLoaderFactoryWithBuilder::
    ~PendingSharedURLLoaderFactoryWithBuilder() = default;

scoped_refptr<SharedURLLoaderFactory>
PendingSharedURLLoaderFactoryWithBuilder::CreateFactory() {
  return std::move(factory_builder_)
      .Finish(
          SharedURLLoaderFactory::Create(std::move(terminal_pending_factory_)));
}

}  // namespace network
