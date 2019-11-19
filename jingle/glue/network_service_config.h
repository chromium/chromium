// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Configuration information for talking to the network service.

#ifndef JINGLE_GLUE_NETWORK_SERVICE_CONFIG_H_
#define JINGLE_GLUE_NETWORK_SERVICE_CONFIG_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace jingle_glue {

using GetProxyResolvingSocketFactoryCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>;

struct NetworkServiceConfig {
  NetworkServiceConfig();
  NetworkServiceConfig(const NetworkServiceConfig& other);
  ~NetworkServiceConfig();

  // This will be run on |task_runner|.
  GetProxyResolvingSocketFactoryCallback
      get_proxy_resolving_socket_factory_callback;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_NETWORK_SERVICE_CONFIG_H_
