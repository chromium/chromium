// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_working_set.h"

#include "base/logging.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_response.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

AppCacheWorkingSet::AppCacheWorkingSet() : is_disabled_(false) {}

AppCacheWorkingSet::~AppCacheWorkingSet() {
  DCHECK(caches_.empty());
  DCHECK(groups_.empty());
  DCHECK(groups_by_origin_.empty());
}

void AppCacheWorkingSet::Disable() {
  if (is_disabled_)
    return;
  is_disabled_ = true;
  caches_.clear();
  groups_.clear();
  groups_by_origin_.clear();
  response_infos_.clear();
}

void AppCacheWorkingSet::AddCache(AppCache* cache) {
  if (is_disabled_)
    return;
  DCHECK(cache->cache_id() != blink::mojom::kAppCacheNoCacheId);
  int64_t cache_id = cache->cache_id();
  DCHECK(caches_.find(cache_id) == caches_.end());
  caches_.emplace(cache_id, cache);
}

void AppCacheWorkingSet::RemoveCache(AppCache* cache) {
  caches_.erase(cache->cache_id());
}

void AppCacheWorkingSet::AddGroup(AppCacheGroup* group) {
  if (is_disabled_)
    return;
  const GURL& url = group->manifest_url();
  DCHECK(groups_.find(url) == groups_.end());
  groups_.emplace(url, group);
  groups_by_origin_[url::Origin::Create(url)].emplace(url, group);
}

void AppCacheWorkingSet::RemoveGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  groups_.erase(url);

  const url::Origin origin(url::Origin::Create(url));
  GroupMap* groups_in_origin = GetMutableGroupsInOrigin(origin);
  if (groups_in_origin) {
    groups_in_origin->erase(url);
    if (groups_in_origin->empty())
      groups_by_origin_.erase(origin);
  }
}

void AppCacheWorkingSet::AddResponseInfo(AppCacheResponseInfo* info) {
  if (is_disabled_)
    return;
  DCHECK(info->response_id() != blink::mojom::kAppCacheNoResponseId);
  int64_t response_id = info->response_id();
  DCHECK(response_infos_.find(response_id) == response_infos_.end());
  response_infos_.emplace(response_id, info);
}

void AppCacheWorkingSet::RemoveResponseInfo(AppCacheResponseInfo* info) {
  response_infos_.erase(info->response_id());
}

}  // namespace
