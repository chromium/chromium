// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_browser_main_parts.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_module.h"
#include "net/grit/net_resources.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/shell/browser/shell_plugin_service_filter.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

namespace content {

WebTestBrowserMainParts::WebTestBrowserMainParts(
    const MainFunctionParams& parameters)
    : ShellBrowserMainParts(parameters) {}

WebTestBrowserMainParts::~WebTestBrowserMainParts() {}

void WebTestBrowserMainParts::InitializeBrowserContexts() {
  set_browser_context(new WebTestBrowserContext(false));
  set_off_the_record_browser_context(nullptr);
}

void WebTestBrowserMainParts::InitializeMessageLoopContext() {
#if BUILDFLAG(ENABLE_PLUGINS)
  PluginService* plugin_service = PluginService::GetInstance();
  plugin_service_filter_.reset(new ShellPluginServiceFilter);
  plugin_service->SetFilter(plugin_service_filter_.get());
#endif
}

}  // namespace content
