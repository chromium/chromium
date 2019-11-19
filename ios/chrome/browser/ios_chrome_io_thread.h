// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_

#include <memory>

#include "ios/components/io_thread/ios_io_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"

class PrefService;

namespace net {
class NetLog;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace web {
class NetworkContextOwner;
}

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
class IOSChromeIOThread : public io_thread::IOSIOThread {
 public:
  IOSChromeIOThread(PrefService* local_state, net::NetLog* net_log);
  ~IOSChromeIOThread() override;

  network::mojom::NetworkContext* GetSystemNetworkContext();

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();

  void NetworkTearDown();

 protected:
  // io_thread::IOSIOThread overrides
  std::unique_ptr<net::NetworkDelegate> CreateSystemNetworkDelegate() override;
  std::string GetChannelString() const override;

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;

  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<web::NetworkContextOwner> network_context_owner_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeIOThread);
};

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_
