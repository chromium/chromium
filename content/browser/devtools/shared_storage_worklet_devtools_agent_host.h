// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_AGENT_HOST_H_

#include <string>

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class SharedStorageWorkletHost;

class CONTENT_EXPORT SharedStorageWorkletDevToolsAgentHost
    : public DevToolsAgentHostImpl {
 public:
  SharedStorageWorkletDevToolsAgentHost(
      SharedStorageWorkletHost& worklet_host,
      const base::UnguessableToken& devtools_worklet_token);

  SharedStorageWorkletDevToolsAgentHost(
      const SharedStorageWorkletDevToolsAgentHost&) = delete;
  SharedStorageWorkletDevToolsAgentHost& operator=(
      const SharedStorageWorkletDevToolsAgentHost&) = delete;

  void WorkletReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver);
  void WorkletDestroyed();

  bool IsRelevantTo(RenderFrameHostImpl* frame);

 private:
  FRIEND_TEST_ALL_PREFIXES(SharedStorageWorkletDevToolsAgentHostTest,
                           BasicAttributes);

  ~SharedStorageWorkletDevToolsAgentHost() override;

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  // DevToolsAgentHostImpl override.
  protocol::TargetAutoAttacher* auto_attacher() override;
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;

  std::unique_ptr<protocol::TargetAutoAttacher> auto_attacher_;
  raw_ptr<SharedStorageWorkletHost> worklet_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_AGENT_HOST_H_
