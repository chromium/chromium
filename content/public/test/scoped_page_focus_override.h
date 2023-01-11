// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_PAGE_FOCUS_OVERRIDE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_PAGE_FOCUS_OVERRIDE_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

class WebContents;

// This calls into devtools to enable focus emulation for the given WebContents.
// As long as this class is alive any calls to Document::hasFocus() will return
// true which emulates focus on that document.
class ScopedPageFocusOverride : public DevToolsAgentHostClient {
 public:
  explicit ScopedPageFocusOverride(WebContents* web_contents);
  ScopedPageFocusOverride(const ScopedPageFocusOverride&) = delete;
  ScopedPageFocusOverride& operator=(const ScopedPageFocusOverride&) = delete;
  ~ScopedPageFocusOverride() override;

 protected:
  // DevToolsAgentHostClient:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;

 private:
  void SetFocusEmulationEnabled(bool enabled);

  int last_sent_id_ = 0;
  base::OnceClosure run_loop_quit_closure_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_PAGE_FOCUS_OVERRIDE_H_
