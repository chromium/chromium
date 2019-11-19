// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "net/http/http_status_code.h"

namespace base {
class DictionaryValue;
class Thread;
class Value;
}

namespace content {
class DevToolsManagerDelegate;
class DevToolsSocketFactory;
}

namespace net {
class IPEndPoint;
class HttpServerRequestInfo;
}

namespace content {

class DevToolsAgentHostClientImpl;
class ServerWrapper;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:
  // Takes ownership over |socket_factory|.
  // |delegate| is only accessed on UI thread.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  DevToolsHttpHandler(
      DevToolsManagerDelegate* delegate,
      std::unique_ptr<DevToolsSocketFactory> server_socket_factory,
      const base::FilePath& active_port_output_directory,
      const base::FilePath& debug_frontend_dir);
  ~DevToolsHttpHandler();

 private:
  friend class ServerWrapper;
  friend void ServerStartedOnUI(
      base::WeakPtr<DevToolsHttpHandler> handler,
      base::Thread* thread,
      ServerWrapper* server_wrapper,
      DevToolsSocketFactory* socket_factory,
      std::unique_ptr<net::IPEndPoint> ip_address);

  void OnJsonRequest(int connection_id,
                     const net::HttpServerRequestInfo& info);
  void RespondToJsonList(int connection_id,
                         const std::string& host,
                         DevToolsAgentHost::List agent_hosts);
  void OnDiscoveryPageRequest(int connection_id);
  void OnFrontendResourceRequest(int connection_id, const std::string& path);
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnWebSocketMessage(int connection_id, const std::string& data);
  void OnClose(int connection_id);

  void ServerStarted(std::unique_ptr<base::Thread> thread,
                     std::unique_ptr<ServerWrapper> server_wrapper,
                     std::unique_ptr<DevToolsSocketFactory> socket_factory,
                     std::unique_ptr<net::IPEndPoint> ip_address);

  void SendJson(int connection_id,
                net::HttpStatusCode status_code,
                base::Value* value,
                const std::string& message);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  void DecompressAndSendJsonProtocol(int connection_id);

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(
      scoped_refptr<DevToolsAgentHost> agent_host,
      const std::string& target_id,
      const std::string& host);

  std::unique_ptr<base::DictionaryValue> SerializeDescriptor(
      scoped_refptr<DevToolsAgentHost> agent_host,
      const std::string& host);

  // The thread used by the devtools handler to run server socket.
  std::unique_ptr<base::Thread> thread_;
  std::string browser_guid_;
  std::unique_ptr<ServerWrapper> server_wrapper_;
  std::unique_ptr<net::IPEndPoint> server_ip_address_;
  using ConnectionToClientMap =
      std::map<int, std::unique_ptr<DevToolsAgentHostClientImpl>>;
  ConnectionToClientMap connection_to_client_;
  DevToolsManagerDelegate* delegate_;
  std::unique_ptr<DevToolsSocketFactory> socket_factory_;
  base::WeakPtrFactory<DevToolsHttpHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_H_
