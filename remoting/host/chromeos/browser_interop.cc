// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/browser_interop.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
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

// Creates the client cert store of `profile`. The store, and the identities
// and private keys that it returns, are backed by the profile's keyed services
// and by Kcer, so they can only be used on the UI thread.
std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
    base::WeakPtr<Profile> profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile) {
    LOG(ERROR) << "Failed to create client cert store since profile is gone";
    return nullptr;
  }
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(profile.get());
  if (!profile_network_context_service) {
    LOG(ERROR) << "Failed to get ProfileNetworkContextServiceFactory for "
                  "browser context: "
               << profile->UniqueId();
    return nullptr;
  }
  auto client_cert_store =
      profile_network_context_service->CreateClientCertStore();
  if (!client_cert_store) {
    LOG(ERROR) << "Failed to create ClientCertStore for browser context: "
               << profile->UniqueId();
  }
  return client_cert_store;
}

ChromotingHostContext::CreateClientCertStoreCallback
MakeCreateClientCertStoreCallback(content::BrowserContext* browser_context) {
  if (!browser_context) {
    VLOG(1) << "BrowserContext is null; client certificates will not be "
               "provided.";
    return base::BindRepeating(
        []() -> std::unique_ptr<net::ClientCertStore> { return nullptr; });
  }
  return base::BindRepeating(
      &CreateClientCertStore,
      Profile::FromBrowserContext(browser_context)->GetWeakPtr());
}

}  // namespace

std::unique_ptr<ChromotingHostContext>
BrowserInterop::CreateChromotingHostContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ChromotingHostContext::CreateForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      g_browser_process->shared_url_loader_factory(),
      MakeCreateClientCertStoreCallback(browser_context));
}

std::unique_ptr<PolicyWatcher> BrowserInterop::CreatePolicyWatcher() {
  return PolicyWatcher::CreateWithPolicyService(
      g_browser_process->policy_service());
}

}  // namespace remoting
