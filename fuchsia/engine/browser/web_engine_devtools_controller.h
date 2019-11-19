// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_CONTROLLER_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_CONTROLLER_H_

#include "base/callback.h"
#include "content/public/browser/devtools_agent_host.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

// Manages the DevTools remote-debugging server.
class WebEngineDevToolsController {
 public:
  virtual ~WebEngineDevToolsController() = default;

  // Returns a controller based on the features requested in |command_line|.
  static std::unique_ptr<WebEngineDevToolsController> CreateFromCommandLine(
      const base::CommandLine& command_line);

  // Called by the Context to signal its creation.
  virtual void OnContextCreated() = 0;

  // Called by the Context to signal its destruction.
  virtual void OnContextDestroyed() = 0;

  // Called by the Context to signal a new Frame was created. |user_debugging|
  // should be set to true when debugging was requested from the user API.
  // Returns false if |contents| is not user-debuggable despite |user_debugging|
  // being set.
  virtual bool OnFrameCreated(content::WebContents* contents,
                              bool user_debugging) = 0;

  // Called by Frames to signal a load has completed.
  virtual void OnFrameLoaded(content::WebContents* contents) = 0;

  // Called by Frames to signal they are being deleted.
  virtual void OnFrameDestroyed(content::WebContents* contents) = 0;

  // Called by the WebEngineDevToolsManagerDelegate.
  virtual content::DevToolsAgentHost::List RemoteDebuggingTargets() = 0;

  // Invokes |callback| as soon as a user-mode DevTools port becomes available.
  // |callback| receives zero if user-mode DevTools is permanently unavailable.
  virtual void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) = 0;
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_CONTROLLER_H_
