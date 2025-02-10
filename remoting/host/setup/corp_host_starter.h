// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_CORP_HOST_STARTER_H_
#define REMOTING_HOST_SETUP_CORP_HOST_STARTER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace net {
class ClientCertStore;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

class HostStarter;

extern std::unique_ptr<HostStarter> ProvisionCorpMachine(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<net::ClientCertStore> client_cert_store);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_CORP_HOST_STARTER_H_
