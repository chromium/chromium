// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/browser_interop.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/policy_watcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
    content::BrowserContext* browser_context) {
  if (!browser_context) {
    LOG(ERROR)
        << "Failed to create client cert store since browser context is null";
    return nullptr;
  }
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser_context);
  if (!profile_network_context_service) {
    LOG(ERROR) << "Failed to get ProfileNetworkContextServiceFactory for "
                  "browser context: "
               << browser_context->UniqueId();
    return nullptr;
  }
  auto client_cert_store =
      profile_network_context_service->CreateClientCertStore();
  if (!client_cert_store) {
    LOG(ERROR) << "Failed to create ClientCertStore for browser context: "
               << browser_context->UniqueId();
  }
  return client_cert_store;
}

}  // namespace

std::unique_ptr<ChromotingHostContext>
BrowserInterop::CreateChromotingHostContext(
    content::BrowserContext* browser_context) {
  return ChromotingHostContext::CreateForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      g_browser_process->shared_url_loader_factory(),
      base::BindRepeating(&CreateClientCertStore,
                          base::Unretained(browser_context)));
}

std::unique_ptr<PolicyWatcher> BrowserInterop::CreatePolicyWatcher() {
  return PolicyWatcher::CreateWithPolicyService(
      g_browser_process->policy_service());
}

}  // namespace remoting
