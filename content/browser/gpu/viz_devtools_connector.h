// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_VIZ_DEVTOOLS_CONNECTOR_H_
#define CONTENT_BROWSER_GPU_VIZ_DEVTOOLS_CONNECTOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace content {

// Creates a TCP server socket, and uses it to connect the viz devtools server
// on the gpu process.
class CONTENT_EXPORT VizDevToolsConnector {
 public:
  VizDevToolsConnector();
  ~VizDevToolsConnector();

  void ConnectVizDevTools();

 private:
  void OnVizDevToolsSocketCreated(
      mojo::PendingRemote<network::mojom::TCPServerSocket> socket,
      int result,
      int port);

  base::WeakPtrFactory<VizDevToolsConnector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VizDevToolsConnector);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_VIZ_DEVTOOLS_CONNECTOR_H_
