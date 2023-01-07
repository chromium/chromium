// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PARTS_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "build/build_config.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "ppapi/buildflags/buildflags.h"

namespace content {

class ShellPluginServiceFilter;

class WebTestBrowserMainParts : public ShellBrowserMainParts {
 public:
  explicit WebTestBrowserMainParts();

  WebTestBrowserMainParts(const WebTestBrowserMainParts&) = delete;
  WebTestBrowserMainParts& operator=(const WebTestBrowserMainParts&) = delete;

  ~WebTestBrowserMainParts() override;

 private:
  // ShellBrowserMainParts overrides.
  void InitializeBrowserContexts() override;
  void InitializeMessageLoopContext() override;
  std::unique_ptr<ShellPlatformDelegate> CreateShellPlatformDelegate() override;

#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<ShellPluginServiceFilter> plugin_service_filter_;
#endif
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PARTS_H_
