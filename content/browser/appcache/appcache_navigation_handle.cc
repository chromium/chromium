// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// Map of AppCache host id to the AppCacheNavigationHandle instance.
// Accessed on the UI thread only.
// TODO(nhiroki): base::LazyInstance is deprecated. Use base::NoDestructor
// instead.
using AppCacheHandleMap =
    std::map<base::UnguessableToken, content::AppCacheNavigationHandle*>;
base::LazyInstance<AppCacheHandleMap>::DestructorAtExit g_appcache_handle_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AppCacheNavigationHandle::AppCacheNavigationHandle(
    ChromeAppCacheService* appcache_service,
    int process_id)
    : appcache_host_id_(base::UnguessableToken::Create()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  precreated_host_ = std::make_unique<AppCacheHost>(
      appcache_host_id_, process_id, MSG_ROUTING_NONE, mojo::NullRemote(),
      static_cast<AppCacheServiceImpl*>(appcache_service));

  DCHECK(g_appcache_handle_map.Get().find(appcache_host_id_) ==
         g_appcache_handle_map.Get().end());
  g_appcache_handle_map.Get()[appcache_host_id_] = this;
}

AppCacheNavigationHandle::~AppCacheNavigationHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_appcache_handle_map.Get().erase(appcache_host_id_);
}

// static
std::unique_ptr<AppCacheHost> AppCacheNavigationHandle::TakePrecreatedHost(
    const base::UnguessableToken& host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto index = g_appcache_handle_map.Get().find(host_id);
  if (index != g_appcache_handle_map.Get().end()) {
    AppCacheNavigationHandle* instance = index->second;
    DCHECK(instance);
    return std::move(instance->precreated_host_);
  }
  return nullptr;
}

}  // namespace content
