// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MOJOM_DEVTOOLS_AGENT_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_MOJOM_DEVTOOLS_AGENT_HOST_DELEGATE_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

// Describes an interface for implementing a mojom devtools agent.
class MojomDevToolsAgentHostDelegate {
 public:
  virtual ~MojomDevToolsAgentHostDelegate() = default;

  // Returns agent host type.
  virtual std::string GetType() const = 0;

  // Returns agent host title.
  virtual std::string GetTitle() const = 0;

  // Returns url associated with agent host.
  virtual GURL GetURL() const = 0;

  // Activates agent host.
  virtual bool Activate() = 0;

  // Reloads agent host.
  virtual void Reload() = 0;

  // Reloads agent host.
  virtual bool Close() = 0;

  // Whether to use force IO session when attaching to agent.
  virtual bool ForceIOSession() = 0;

  // Binds the remote agent host to the receiver agent.
  virtual void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent>
          agent_receiver) = 0;
};
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MOJOM_DEVTOOLS_AGENT_HOST_DELEGATE_H_
