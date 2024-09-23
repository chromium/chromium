// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

namespace content {

class RenderFrameHostImpl;
class DevToolsSession;

namespace protocol {

class DevToolsDomainHandler {
 public:
  explicit DevToolsDomainHandler(const std::string& name);

  DevToolsDomainHandler(const DevToolsDomainHandler&) = delete;
  DevToolsDomainHandler& operator=(const DevToolsDomainHandler&) = delete;

  virtual ~DevToolsDomainHandler();

  virtual void SetRenderer(int process_host_id,
                           RenderFrameHostImpl* frame_host);
  virtual void Wire(UberDispatcher* dispatcher);
  void SetSession(DevToolsSession* session);
  virtual Response Disable();

  const std::string& name() const;

 protected:
  DevToolsSession* session();

 private:
  std::string name_;
  raw_ptr<DevToolsSession> session_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_
