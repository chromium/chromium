// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_AGENT_HOST_CLIENT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_AGENT_HOST_CLIENT_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "headless/public/headless_devtools_channel.h"
#include "headless/public/headless_export.h"

namespace headless {

class HEADLESS_EXPORT HeadlessDevToolsAgentHostClient
    : public content::DevToolsAgentHostClient,
      public HeadlessDevToolsChannel {
 public:
  explicit HeadlessDevToolsAgentHostClient(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~HeadlessDevToolsAgentHostClient() override;

  // content::DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> json_message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;

  // HeadlessDevToolsChannel implementation.
  void SetClient(HeadlessDevToolsChannel::Client* client) override;
  void SendProtocolMessage(base::span<const uint8_t> message) override;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  HeadlessDevToolsChannel::Client* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsAgentHostClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_AGENT_HOST_CLIENT_H_
