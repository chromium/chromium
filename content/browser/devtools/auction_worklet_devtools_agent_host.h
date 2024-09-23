// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_AUCTION_WORKLET_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_AUCTION_WORKLET_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace content {

class DebuggableAuctionWorklet;

class AuctionWorkletDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  static bool IsRelevantTo(RenderFrameHostImpl* frame,
                           DebuggableAuctionWorklet* candidate);

 private:
  friend class AuctionWorkletDevToolsAgentHostManager;

  static scoped_refptr<AuctionWorkletDevToolsAgentHost> Create(
      DebuggableAuctionWorklet* worklet);

  explicit AuctionWorkletDevToolsAgentHost(DebuggableAuctionWorklet* worklet);
  ~AuctionWorkletDevToolsAgentHost() override;

  // DevToolsAgentHost override.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  void Reload() override;
  std::string GetParentId() override;
  BrowserContext* GetBrowserContext() override;

  // Called by WorkerDevToolsAgentHostManager to specify the worklet got
  // destroyed.
  void WorkletDestroyed();

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;

  raw_ptr<DebuggableAuctionWorklet> worklet_ = nullptr;
  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> associated_agent_remote_;
};

class AuctionWorkletDevToolsAgentHostManager
    : public DebuggableAuctionWorkletTracker::Observer {
 public:
  // Both of these append to `out`.
  void GetAll(DevToolsAgentHost::List* out);
  void GetAllForFrame(RenderFrameHostImpl* frame, DevToolsAgentHost::List* out);

  scoped_refptr<AuctionWorkletDevToolsAgentHost> GetOrCreateFor(
      DebuggableAuctionWorklet* worklet);

  static AuctionWorkletDevToolsAgentHostManager& GetInstance();

 private:
  friend class AuctionWorkletDevToolsAgentHost;
  friend class base::NoDestructor<AuctionWorkletDevToolsAgentHostManager>;

  AuctionWorkletDevToolsAgentHostManager();
  ~AuctionWorkletDevToolsAgentHostManager() override;

  // DebuggableAuctionWorkletTracker::Observer implementation.
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override;
  void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) override;

  std::map<DebuggableAuctionWorklet*,
           scoped_refptr<AuctionWorkletDevToolsAgentHost>>
      hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_AUCTION_WORKLET_DEVTOOLS_AGENT_HOST_H_
