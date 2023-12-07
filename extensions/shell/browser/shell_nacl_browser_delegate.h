// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/nacl/browser/nacl_browser_delegate.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class InfoMap;

// A lightweight NaClBrowserDelegate for app_shell. Only supports a single
// BrowserContext.
class ShellNaClBrowserDelegate : public NaClBrowserDelegate {
 public:
  // Uses |context| to look up extensions via InfoMap on the IO thread.
  explicit ShellNaClBrowserDelegate(content::BrowserContext* context);

  ShellNaClBrowserDelegate(const ShellNaClBrowserDelegate&) = delete;
  ShellNaClBrowserDelegate& operator=(const ShellNaClBrowserDelegate&) = delete;

  ~ShellNaClBrowserDelegate() override;

  // NaClBrowserDelegate overrides:
  void ShowMissingArchInfobar(int render_process_id,
                              int render_frame_id) override;
  bool DialogsAreSuppressed() override;
  bool GetCacheDirectory(base::FilePath* cache_dir) override;
  bool GetPluginDirectory(base::FilePath* plugin_dir) override;
  bool GetPnaclDirectory(base::FilePath* pnacl_dir) override;
  bool GetUserDirectory(base::FilePath* user_dir) override;
  std::string GetVersionString() const override;
  ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) override;
  MapUrlToLocalFilePathCallback GetMapUrlToLocalFilePathCallback(
      const base::FilePath& profile_directory) override;
  void SetDebugPatterns(const std::string& debug_patterns) override;
  bool URLMatchesDebugPatterns(const GURL& manifest_url) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;  // Not owned.
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_
