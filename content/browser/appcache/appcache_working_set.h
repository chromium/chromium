// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_WORKING_SET_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_WORKING_SET_H_

#include <stdint.h>

#include <map>
#include <unordered_map>

#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AppCache;
class AppCacheGroup;
class AppCacheResponseInfo;

// Represents the working set of appcache object instances
// currently in memory.
class CONTENT_EXPORT AppCacheWorkingSet {
 public:
  using GroupMap = std::map<GURL, AppCacheGroup*>;

  AppCacheWorkingSet();
  ~AppCacheWorkingSet();

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  void AddCache(AppCache* cache);
  void RemoveCache(AppCache* cache);
  AppCache* GetCache(int64_t id) {
    auto it = caches_.find(id);
    return (it != caches_.end()) ? it->second : nullptr;
  }

  void AddGroup(AppCacheGroup* group);
  void RemoveGroup(AppCacheGroup* group);
  AppCacheGroup* GetGroup(const GURL& manifest_url) {
    auto it = groups_.find(manifest_url);
    return (it != groups_.end()) ? it->second : nullptr;
  }

  const GroupMap* GetGroupsInOrigin(const url::Origin& origin) {
    return GetMutableGroupsInOrigin(origin);
  }

  void AddResponseInfo(AppCacheResponseInfo* response_info);
  void RemoveResponseInfo(AppCacheResponseInfo* response_info);
  AppCacheResponseInfo* GetResponseInfo(int64_t id) {
    auto it = response_infos_.find(id);
    return (it != response_infos_.end()) ? it->second : nullptr;
  }

 private:
  GroupMap* GetMutableGroupsInOrigin(const url::Origin& origin) {
    auto it = groups_by_origin_.find(origin);
    return (it != groups_by_origin_.end()) ? &it->second : nullptr;
  }

  std::unordered_map<int64_t, AppCache*> caches_;
  GroupMap groups_;
  // origin -> (manifest -> group)
  std::map<url::Origin, GroupMap> groups_by_origin_;
  std::unordered_map<int64_t, AppCacheResponseInfo*> response_infos_;
  bool is_disabled_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_WORKING_SET_H_
