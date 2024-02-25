// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRANSITIONAL_URL_LOADER_FACTORY_OWNER_H_
#define SERVICES_NETWORK_TRANSITIONAL_URL_LOADER_FACTORY_OWNER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {

class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;

// This class is intended for stand-alone executables that use net/ and also
// components that use Network Service APIs, and therefore need a
// SharedURLLoaderFactory for fetching. This provides it on top of the existing
// URLRequestContextGetter. This should not be used within the browser process.
//
// All of the methods must be called from the same sequence, which may be
// different from |url_request_context_getter|'s network thread.
class COMPONENT_EXPORT(NETWORK_SERVICE) TransitionalURLLoaderFactoryOwner {
 public:
  // |this| must outlive the URLRequestContext underlying
  // |url_request_context_getter|.
  explicit TransitionalURLLoaderFactoryOwner(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      bool is_trusted = false);

  TransitionalURLLoaderFactoryOwner(const TransitionalURLLoaderFactoryOwner&) =
      delete;
  TransitionalURLLoaderFactoryOwner& operator=(
      const TransitionalURLLoaderFactoryOwner&) = delete;

  ~TransitionalURLLoaderFactoryOwner();

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  network::mojom::NetworkContext* GetNetworkContext();

  // If this is called, any creation, use, or destruction of a
  // TransitionalURLLoaderFactoryOwner will DCHECK-fail.
  static void DisallowUsageInProcess();

 private:
  class Core;

  static base::AtomicFlag& disallowed_in_process();

  std::unique_ptr<Core> core_;  // deleted cross-thread
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;
  bool is_trusted_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRANSITIONAL_URL_LOADER_FACTORY_OWNER_H_
