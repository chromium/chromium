// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PIPE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PIPE_HANDLER_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

class PipeReaderBase;
class PipeWriterBase;

class DevToolsPipeHandler : public DevToolsAgentHostClient {
 public:
  DevToolsPipeHandler(int read_fd,
                      int write_fd,
                      base::OnceClosure on_disconnect);

  DevToolsPipeHandler(const DevToolsPipeHandler&) = delete;
  DevToolsPipeHandler& operator=(const DevToolsPipeHandler&) = delete;

  ~DevToolsPipeHandler() override;

  void HandleMessage(std::vector<uint8_t> message);
  void OnDisconnect();

  // DevToolsAgentHostClient overrides
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  bool UsesBinaryProtocol() override;
  bool AllowUnsafeOperations() override;
  std::string GetTypeForMetrics() override;

  void Shutdown();

 private:
  enum class ProtocolMode {
    // Legacy text protocol format with messages separated by \0's.
    kASCIIZ,
    // Experimental (!) CBOR (RFC 7049) based binary format.
    kCBOR
  };

  ProtocolMode mode_;
  base::OnceClosure on_disconnect_;

  std::unique_ptr<PipeReaderBase> pipe_reader_;
  std::unique_ptr<PipeWriterBase> pipe_writer_;
  scoped_refptr<DevToolsAgentHost> browser_target_;
  int read_fd_;
  int write_fd_;
  bool shutting_down_ = false;
  base::WeakPtrFactory<DevToolsPipeHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PIPE_HANDLER_H_
