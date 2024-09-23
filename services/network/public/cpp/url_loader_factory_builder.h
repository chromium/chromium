// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_FACTORY_BUILDER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_FACTORY_BUILDER_H_

#include <tuple>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// URLLoaderFactoryBuilder allows to build a URLLoaderFactory where requests
// go through interceptors/proxying URLLoaderFactories and finally processed by
// the terminal URLLoaderFactory.
// A request received by the resulting factory will be processed by the
// interceptors (in the order of `Append()` calls), and then finally passed to
// the terminal (given by `terminal_args` of `Finish*()` methods).
//
// URLLoaderFactoryBuilder connects pipes and URLLoaderFactories eagerly, and
// tries to minimize additional intermediate objects between them.
//
// Basic Usage:
//
//   // Start with no interceptors.
//   URLLoaderFactoryBuilder factory_builder;
//
//   // Add Interceptor 1.
//   mojo::PendingReceiver<mojom::URLLoaderFactory> receiver1;
//   mojo::PendingRemote<mojom::URLLoaderFactory> remote1;
//   std::tie(receiver1, remote1) = factory_builder.Append();
//   ...; // Connect the pipes so that the interceptor handles incoming requests
//        // from `receiver1` and forwards processed requests to `remote1`.
//
//   // Add Interceptor 2.
//   auto [receiver2, remote2] = factory_builder.Append();
//   ...; // Connect the pipes as well.
//
//   // Finish the builder by connecting to the terminal URLLoaderFactory,
//   // getting the resulting URLLoaderFactory.
//   // `result->CreateLoaderAndStart()` will go through Interceptor 1,
//   // Interceptor 2 (in the order of `Append()` calls) then `terminal`.
//   scoped_refptr<SharedURLLoaderFactory> terminal = ...;
//   scoped_refptr<SharedURLLoaderFactory> result =
//       std::move(factory_builder).Finish(std::move(terminal));
//
// See `url_loader_factory_builder_unittest.cc` for more concrete examples.
class COMPONENT_EXPORT(NETWORK_CPP) URLLoaderFactoryBuilder final {
 public:
  // Creates a builder, initially with no interceptors.
  URLLoaderFactoryBuilder();
  ~URLLoaderFactoryBuilder();

  URLLoaderFactoryBuilder(const URLLoaderFactoryBuilder&) = delete;
  URLLoaderFactoryBuilder& operator=(const URLLoaderFactoryBuilder&) = delete;

  URLLoaderFactoryBuilder(URLLoaderFactoryBuilder&&);
  URLLoaderFactoryBuilder& operator=(URLLoaderFactoryBuilder&&);

  // Adds an interceptor.
  // Caller should bind the returned `PendingReceiver` and `PendingRemote`.
  [[nodiscard]] std::tuple<mojo::PendingReceiver<mojom::URLLoaderFactory>,
                           mojo::PendingRemote<mojom::URLLoaderFactory>>
  Append();

  // `Finish()` methods finalize the builder by connecting the builder to the
  // mojom::URLLoaderFactory-ish terminal (`terminal_args`).
  // See `ConnectTerminal()` methods for supported terminal `Args`.

  // This `Finish()` variant returns the resulting factory as a
  // mojom::URLLoaderFactory-ish `OutType`.
  // See `WrapAs` methods for supported `OutType`.
  template <typename OutType = scoped_refptr<SharedURLLoaderFactory>,
            typename... Args>
  [[nodiscard]] OutType Finish(Args... terminal_args) && {
    if (IsEmpty()) {
      // The builder has no interceptors, so just forward the terminal.
      return WrapAs<OutType>(std::forward<Args>(terminal_args)...);
    }
    // Connect `head_` -(interceptors)-> `tail_` -> the terminal, and return the
    // head.
    ConnectTerminal(std::move(tail_), std::forward<Args>(terminal_args)...);
    return WrapAs<OutType>(std::move(head_));
  }

  // This `Finish()` variant connects the resulting factory to the given
  // `PendingReceiver`.
  // `OutType` is only for consistency with other `Finish()` variants for
  // `content/browser/loader/url_loader_factory_utils.cc` callers.
  template <typename OutType = void, typename... Args>
  void Finish(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
              Args... terminal_args) && {
    if (IsEmpty()) {
      // The builder has no interceptors, so connect `receiver` directly to the
      // terminal.
      ConnectTerminal(std::move(receiver),
                      std::forward<Args>(terminal_args)...);
      return;
    }
    // Connect `receiver` -> `head_` -(interceptors)-> `tail_` -> the terminal.
    ConnectTerminal(std::move(tail_), std::forward<Args>(terminal_args)...);
    // `head_` can be fused because it is an internally created mojo endpoint
    // that is not yet written.
    CHECK(mojo::FusePipes(std::move(receiver), std::move(head_)));
  }

 private:
  bool IsEmpty() const;

  // ----------------------------------------------------------------
  // `WrapAs`: Helpers to convert mojom::URLLoaderFactory-ish types.
  template <typename OutType, typename InType>
  static OutType WrapAs(InType in) {
    return std::move(in);
  }

  template <typename OutType>
  static OutType WrapAs(mojom::NetworkContext* context,
                        mojom::URLLoaderFactoryParamsPtr factory_param) {
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    context->CreateURLLoaderFactory(remote.InitWithNewPipeAndPassReceiver(),
                                    std::move(factory_param));
    return WrapAs<OutType>(std::move(remote));
  }

  // ----------------------------------------------------------------
  // `ConnectTerminal`: Connect the `PendingReceiver` endpoint to a
  // mojom::URLLoaderFactory-ish terminal.
  static void ConnectTerminal(
      mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
      mojo::Remote<mojom::URLLoaderFactory> terminal_factory);
  static void ConnectTerminal(
      mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
      mojo::PendingRemote<mojom::URLLoaderFactory> terminal_factory);
  static void ConnectTerminal(
      mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
      scoped_refptr<SharedURLLoaderFactory> terminal_factory);
  static void ConnectTerminal(
      mojo::PendingReceiver<mojom::URLLoaderFactory> pending_receiver,
      mojom::NetworkContext* terminal_context,
      mojom::URLLoaderFactoryParamsPtr factory_param);

  // If `head_` and `tail_` are both valid, they hold the head/tail of the
  // chained interceptors, connected like:
  // `head_`->(Interceptor 1)->(Interceptor 2)->...->`tail_`.
  // If they are both invalid, then there are no interceptors.
  mojo::PendingRemote<mojom::URLLoaderFactory> head_;
  mojo::PendingReceiver<mojom::URLLoaderFactory> tail_;
};

// `PendingRemote` -> `SharedURLLoaderFactory`:
// Wraps by `WrapperSharedURLLoaderFactory`.
template <>
COMPONENT_EXPORT(NETWORK_CPP)
scoped_refptr<SharedURLLoaderFactory> URLLoaderFactoryBuilder::WrapAs(
    mojo::PendingRemote<mojom::URLLoaderFactory> in);

// `SharedURLLoaderFactory` -> `PendingRemote`:
// Creates a new pipe and cloning.
template <>
COMPONENT_EXPORT(NETWORK_CPP)
mojo::PendingRemote<mojom::URLLoaderFactory> URLLoaderFactoryBuilder::WrapAs(
    scoped_refptr<SharedURLLoaderFactory> in);

// Similar to PendingSharedURLLoaderFactory, but also goes through
// `URLLoaderFactoryBuilder` when constructing the factory.
class COMPONENT_EXPORT(NETWORK_CPP)
    PendingSharedURLLoaderFactoryWithBuilder final
    : public PendingSharedURLLoaderFactory {
 public:
  PendingSharedURLLoaderFactoryWithBuilder(
      URLLoaderFactoryBuilder factory_builder,
      std::unique_ptr<PendingSharedURLLoaderFactory> terminal_pending_factory);
  ~PendingSharedURLLoaderFactoryWithBuilder() override;

 private:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<SharedURLLoaderFactory> CreateFactory() override;

  URLLoaderFactoryBuilder factory_builder_;
  std::unique_ptr<PendingSharedURLLoaderFactory> terminal_pending_factory_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_FACTORY_BUILDER_H_
