// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_DELEGATE_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_DELEGATE_H_

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Customization of ProcessManager for the extension system embedder.
class ProcessManagerDelegate {
 public:
  virtual ~ProcessManagerDelegate() {}

  // Returns true if the embedder allows background pages for the given
  // |context|.
  virtual bool AreBackgroundPagesAllowedForContext(
      content::BrowserContext* context) const = 0;

  // Returns true if the embedder allows background pages for the given
  // |context|, and a given |extension|.
  virtual bool IsExtensionBackgroundPageAllowed(
      content::BrowserContext* context,
      const Extension& extension) const = 0;

  // Returns true if the embedder wishes to defer starting up the renderers for
  // extension background pages. If the embedder returns true it must call
  // ProcessManager::MaybeCreateStartupBackgroundHosts() when it is ready. See
  // ChromeProcessManagerDelegate for examples of how this is useful.
  virtual bool DeferCreatingStartupBackgroundHosts(
      content::BrowserContext* context) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_DELEGATE_H_
