// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
class ExtensionHost;
class ProcessManager;

class ProcessManagerObserver : public base::CheckedObserver {
 public:
  // Called immediately after an extension background host is started. This
  // corresponds with the loading of background hosts immediately after profile
  // startup.
  virtual void OnBackgroundHostStartup(const Extension* extension) {}

  // Called immediately after an ExtensionHost for an extension is created.
  // This corresponds with any time ProcessManager::OnBackgroundHostCreated is
  // called.
  virtual void OnBackgroundHostCreated(ExtensionHost* host) {}

  // Called immediately after the extension background host is destroyed.
  virtual void OnBackgroundHostClose(const std::string& extension_id) {}

  // Called when a RenderFrameHost has been registered in an extension process.
  virtual void OnExtensionFrameRegistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) {}

  // Called when a RenderFrameHost is no longer part of an extension process.
  virtual void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) {}

  // Called when the observed ProcessManager is shutting down.
  virtual void OnProcessManagerShutdown(ProcessManager* manager) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
