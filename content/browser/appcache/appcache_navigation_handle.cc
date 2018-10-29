// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {
// Used to generate the host id for a navigation initiated by the browser.
// Starts at -1 and keeps going down.
static int g_next_appcache_host_id = -1;
}

namespace content {

AppCacheNavigationHandle::AppCacheNavigationHandle(
    ChromeAppCacheService* appcache_service)
    : appcache_host_id_(g_next_appcache_host_id--),
      core_(std::make_unique<AppCacheNavigationHandleCore>(appcache_service,
                                                           appcache_host_id_)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AppCacheNavigationHandleCore::Initialize,
                     base::Unretained(core_.get())));
}

AppCacheNavigationHandle::~AppCacheNavigationHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delete the AppCacheNavigationHandleCore on the IO thread.
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, core_.release());
}

}  // namespace content
