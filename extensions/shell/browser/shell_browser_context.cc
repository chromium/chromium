// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_browser_context.h"

#include <utility>

#include "base/command_line.h"
#include "base/task/post_task.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/common/constants.h"
#include "extensions/shell/browser/shell_browser_main_parts.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_network_delegate.h"
#include "extensions/shell/browser/shell_special_storage_policy.h"
#include "extensions/shell/browser/shell_url_request_context_getter.h"

namespace extensions {

namespace {

bool IgnoreCertificateErrors() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kIgnoreCertificateErrors);
}

}  // namespace

// Create a normal recording browser context. If we used an incognito context
// then app_shell would also have to create a normal context and manage both.
ShellBrowserContext::ShellBrowserContext(
    ShellBrowserMainParts* browser_main_parts)
    : content::ShellBrowserContext(false /* off_the_record */,
                                   nullptr /* net_log */,
                                   true /* delay_services_creation */),
      storage_policy_(new ShellSpecialStoragePolicy),
      browser_main_parts_(browser_main_parts) {}

ShellBrowserContext::~ShellBrowserContext() {
  content::BrowserContext::NotifyWillBeDestroyed(this);
}

content::BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return storage_policy_.get();
}

net::URLRequestContextGetter* ShellBrowserContext::CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!url_request_context_getter());
  // Handle only chrome-extension:// requests.
  InfoMap* extension_info_map =
      browser_main_parts_->extension_system()->info_map();
  (*protocol_handlers)[kExtensionScheme] = CreateExtensionProtocolHandler(
      false /* is_incognito */, extension_info_map);

  set_url_request_context_getter(new ShellURLRequestContextGetter(
      this, IgnoreCertificateErrors(), GetPath(),
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      protocol_handlers, std::move(request_interceptors), nullptr /* net_log */,
      extension_info_map));
  resource_context_->set_url_request_context_getter(
      url_request_context_getter());
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&ShellBrowserContext::InitURLRequestContextOnIOThread,
                 base::Unretained(this)));
  return url_request_context_getter();
}

void ShellBrowserContext::InitURLRequestContextOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // GetURLRequestContext() will create a URLRequestContext if it isn't
  // initialized.
  url_request_context_getter()->GetURLRequestContext();
}

}  // namespace extensions
