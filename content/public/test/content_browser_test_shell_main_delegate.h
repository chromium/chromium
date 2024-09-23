// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "build/chromeos_buildflags.h"
#include "content/shell/app/shell_main_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(erikchen): Move #include to .cc file and forward declare
// chromeos::LacrosService to resolve crbug.com/1195401.
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {

// Acts like normal ShellMainDelegate but inserts behaviour for browser tests.
class ContentBrowserTestShellMainDelegate : public ShellMainDelegate {
 public:
  ContentBrowserTestShellMainDelegate();
  ~ContentBrowserTestShellMainDelegate() override;

  // ContentMainDelegate implementation:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
#endif
  void CreateThreadPool(std::string_view name) override;
  // ShellMainDelegate overrides.
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::LacrosService> lacros_service_;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
