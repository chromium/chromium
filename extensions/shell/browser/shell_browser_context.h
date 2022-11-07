// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "content/shell/browser/shell_browser_context.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace extensions {

// The BrowserContext used by the content, apps and extensions systems in
// app_shell.
class ShellBrowserContext final : public content::ShellBrowserContext {
 public:
  explicit ShellBrowserContext();

  ShellBrowserContext(const ShellBrowserContext&) = delete;
  ShellBrowserContext& operator=(const ShellBrowserContext&) = delete;

  ~ShellBrowserContext() override;

  // content::BrowserContext implementation.
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;

 private:
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
