// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/shell/browser/shell_browser_context.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace extensions {

// The BrowserContext used by the content, apps and extensions systems in
// app_shell.
class ShellBrowserContext final : public content::ShellBrowserContext {
 public:
  explicit ShellBrowserContext();
  ~ShellBrowserContext() override;

  // content::BrowserContext implementation.
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  void SetCorsOriginAccessListForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override;

 private:
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
