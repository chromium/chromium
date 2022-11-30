// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/app/content_main_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class ContentBrowserClient;
class ContentClient;
class ContentRendererClient;
}

namespace extensions {

class ShellMainDelegate : public content::ContentMainDelegate {
 public:
  ShellMainDelegate();

  ShellMainDelegate(const ShellMainDelegate&) = delete;
  ShellMainDelegate& operator=(const ShellMainDelegate&) = delete;

  ~ShellMainDelegate() override;

  // ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  void ProcessExiting(const std::string& process_type) override;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  void ZygoteStarting(std::vector<std::unique_ptr<content::ZygoteForkDelegate>>*
                          delegates) override;
#endif
#if BUILDFLAG(IS_MAC)
  absl::optional<int> PreBrowserMain() override;
#endif

 private:
  // |process_type| is zygote, renderer, utility, etc. Returns true if the
  // process needs data from resources.pak.
  static bool ProcessNeedsResourceBundle(const std::string& process_type);

  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
