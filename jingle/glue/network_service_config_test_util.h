// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper for tests that want to fill in a NetworkServiceConfig

#ifndef JINGLE_GLUE_NETWORK_SERVICE_CONFIG_TEST_UTIL_H_
#define JINGLE_GLUE_NETWORK_SERVICE_CONFIG_TEST_UTIL_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "jingle/glue/network_service_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace base {
class WaitableEvent;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace jingle_glue {

class NetworkServiceConfigTestUtil {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // All public methods must be called on the thread this is created on,
  // but the callback returned by MakeSocketFactoryCallback() is expected to be
  // run on |url_request_context_getter->GetNetworkTaskRunner()|, which can be,
  // but does not have to be, a separare thread. The constructor and destructor
  // can block, but will not spin the event loop.
  explicit NetworkServiceConfigTestUtil(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  explicit NetworkServiceConfigTestUtil(
      NetworkContextGetter network_context_getter);
  ~NetworkServiceConfigTestUtil();

  // Configures |config| to run the result of MakeSocketFactoryCallback()
  // on the network runner of |url_request_context_getter| passed to the
  // constructor.
  void FillInNetworkConfig(NetworkServiceConfig* config);
  GetProxyResolvingSocketFactoryCallback MakeSocketFactoryCallback();

 private:
  static void RequestSocket(
      base::WeakPtr<NetworkServiceConfigTestUtil> instance,
      scoped_refptr<base::SequencedTaskRunner> mojo_runner,
      scoped_refptr<base::SequencedTaskRunner> net_runner,
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);
  static void RequestSocketOnMojoRunner(
      base::WeakPtr<NetworkServiceConfigTestUtil> instance,
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);
  void CreateNetworkContextOnNetworkRunner(
      mojo::PendingReceiver<network::mojom::NetworkContext>
          network_context_receiver,
      base::WaitableEvent* notify);
  void DeleteNetworkContextOnNetworkRunner(base::WaitableEvent* notify);

  scoped_refptr<base::SingleThreadTaskRunner> net_runner_;
  scoped_refptr<base::SequencedTaskRunner> mojo_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  NetworkContextGetter network_context_getter_;
  std::unique_ptr<network::NetworkContext>
      network_context_;  // lives on |net_runner_|
  mojo::Remote<network::mojom::NetworkContext>
      network_context_remote_;  // lives on |mojo_runner_|
  base::WeakPtrFactory<NetworkServiceConfigTestUtil> weak_ptr_factory_{
      this};  // lives on |mojo_runner_|
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_NETWORK_SERVICE_CONFIG_TEST_UTIL_H_
