// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ppapi {
class PpapiPermissions;
}

namespace content {

class BrowserPpapiHostImpl;

class ContentBrowserPepperHostFactory : public ppapi::host::HostFactory {
 public:
  // Non-owning pointer to the filter must outlive this class.
  explicit ContentBrowserPepperHostFactory(BrowserPpapiHostImpl* host);

  ContentBrowserPepperHostFactory(const ContentBrowserPepperHostFactory&) =
      delete;
  ContentBrowserPepperHostFactory& operator=(
      const ContentBrowserPepperHostFactory&) = delete;

  ~ContentBrowserPepperHostFactory() override;

  std::unique_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& message) override;

  // Creates ResourceHost for already accepted TCP |socket|. In the case of
  // failure returns wrapped NULL.
  std::unique_ptr<ppapi::host::ResourceHost> CreateAcceptedTCPSocket(
      PP_Instance instance,
      ppapi::TCPSocketVersion version,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

 private:
  std::unique_ptr<ppapi::host::ResourceHost> CreateNewTCPSocket(
      PP_Instance instance,
      PP_Resource resource,
      ppapi::TCPSocketVersion version);

  const ppapi::PpapiPermissions& GetPermissions() const;

  // Non-owning pointer.
  raw_ptr<BrowserPpapiHostImpl> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_
