// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_DELEGATE_H_

#include "base/callback.h"
#include "base/version.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api {
namespace runtime {
struct PlatformInfo;
}
}

class Extension;
class UpdateObserver;

// This is a delegate interface for chrome.runtime API behavior. Clients must
// vend some implementation of this interface through
// ExtensionsBrowserClient::CreateRuntimeAPIDelegate.
class RuntimeAPIDelegate {
 public:
  struct UpdateCheckResult {
    bool success;
    std::string response;
    std::string version;

    UpdateCheckResult(bool success,
                      const std::string& response,
                      const std::string& version);
  };

  virtual ~RuntimeAPIDelegate() {}

  // The callback given to RequestUpdateCheck.
  using UpdateCheckCallback =
      base::OnceCallback<void(const UpdateCheckResult&)>;

  // Registers an UpdateObserver on behalf of the runtime API.
  virtual void AddUpdateObserver(UpdateObserver* observer) = 0;

  // Unregisters an UpdateObserver on behalf of the runtime API.
  virtual void RemoveUpdateObserver(UpdateObserver* observer) = 0;

  // Reloads an extension.
  virtual void ReloadExtension(const std::string& extension_id) = 0;

  // Requests an extensions update update check. Returns |false| if updates
  // are disabled. Otherwise |callback| is called with the result of the
  // update check.
  virtual bool CheckForUpdates(const std::string& extension_id,
                               UpdateCheckCallback callback) = 0;

  // Navigates the browser to a URL on behalf of the runtime API.
  virtual void OpenURL(const GURL& uninstall_url) = 0;

  // Populates platform info to be provided by the getPlatformInfo function.
  // Returns false iff no info is provided.
  virtual bool GetPlatformInfo(api::runtime::PlatformInfo* info) = 0;

  // Request a restart of the host device. Returns false iff the device
  // will not be restarted.
  virtual bool RestartDevice(std::string* error_message) = 0;

  // Open |extension|'s options page, if it has one. Returns true if an
  // options page was opened, false otherwise. See the docs of the
  // chrome.runtime.openOptionsPage function for the gritty details.
  virtual bool OpenOptionsPage(const Extension* extension,
                               content::BrowserContext* browser_context);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_DELEGATE_H_
