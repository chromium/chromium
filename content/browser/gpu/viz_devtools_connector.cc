// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/viz_devtools_connector.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/viz/common/switches.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

namespace {

void OnSocketCreated(base::OnceCallback<void(int, int)> callback,
                     int result,
                     const base::Optional<net::IPEndPoint>& local_addr) {
  int port = 0;
  if (local_addr)
    port = local_addr->port();
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result, port));
}

void CreateSocketOnUiThread(
    mojo::PendingReceiver<network::mojom::TCPServerSocket>
        server_socket_receiver,
    int port,
    base::OnceCallback<void(int, int)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ui_devtools::UiDevToolsServer::CreateTCPServerSocket(
      std::move(server_socket_receiver),
      GetContentClient()->browser()->GetSystemNetworkContext(), port,
      ui_devtools::UiDevToolsServer::kVizDevtoolsServerTag,
      base::BindOnce(&OnSocketCreated, std::move(callback)));
}

}  // namespace

VizDevToolsConnector::VizDevToolsConnector() {}

VizDevToolsConnector::~VizDevToolsConnector() {}

void VizDevToolsConnector::ConnectVizDevTools() {
  constexpr int kVizDevToolsDefaultPort = 9229;
  mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket;
  int port = ui_devtools::UiDevToolsServer::GetUiDevToolsPort(
      switches::kEnableVizDevTools, kVizDevToolsDefaultPort);
  // Jump to the UI thread to get the network context, create the socket, then
  // jump back to the IO thread to complete the callback.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &CreateSocketOnUiThread,
          server_socket.InitWithNewPipeAndPassReceiver(), port,
          base::BindOnce(&VizDevToolsConnector::OnVizDevToolsSocketCreated,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(server_socket))));
}

void VizDevToolsConnector::OnVizDevToolsSocketCreated(
    mojo::PendingRemote<network::mojom::TCPServerSocket> socket,
    int result,
    int port) {
  viz::mojom::VizDevToolsParamsPtr params =
      viz::mojom::VizDevToolsParams::New();
  params->server_socket = std::move(socket);
  params->server_port = port;
  auto* gpu_process_host = GpuProcessHost::Get();
  if (gpu_process_host)
    gpu_process_host->gpu_host()->ConnectVizDevTools(std::move(params));
}

}  // namespace content
