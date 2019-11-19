// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_

#include <string>
#include <vector>

#include "base/callback.h"

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
struct StorageUsageInfo;
class SessionStorageNamespace;
struct SessionStorageUsageInfo;

// Represents the per-BrowserContext Local Storage data.
class DOMStorageContext {
 public:
  using GetLocalStorageUsageCallback =
      base::OnceCallback<void(const std::vector<StorageUsageInfo>&)>;

  using GetSessionStorageUsageCallback =
      base::OnceCallback<void(const std::vector<SessionStorageUsageInfo>&)>;

  // Returns a collection of origins using local storage to the given callback.
  virtual void GetLocalStorageUsage(GetLocalStorageUsageCallback callback) = 0;

  // Returns a collection of origins using session storage to the given
  // callback.
  virtual void GetSessionStorageUsage(
      GetSessionStorageUsageCallback callback) = 0;

  // Deletes the local storage for the origin of |origin_url|. |callback| is
  // called when the deletion is sent to the database and GetLocalStorageUsage()
  // will not return entries for |origin_url| anymore.
  virtual void DeleteLocalStorage(const url::Origin& origin,
                                  base::OnceClosure callback) = 0;

  // Removes traces of deleted data from the local storage backend.
  virtual void PerformLocalStorageCleanup(base::OnceClosure callback) = 0;

  // Deletes the session storage data identified by |usage_info|.
  virtual void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info,
                                    base::OnceClosure callback) = 0;

  virtual void PerformSessionStorageCleanup(base::OnceClosure callback) = 0;

  // Creates a SessionStorageNamespace with the given |namespace_id|. Used
  // after tabs are restored by session restore. When created, the
  // SessionStorageNamespace with the correct |namespace_id| will be
  // associated with the persisted sessionStorage data.
  virtual scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& namespace_id) = 0;

  // Starts deleting sessionStorages which don't have an associated
  // SessionStorageNamespace alive. Called when SessionStorageNamespaces have
  // been created after a session restore, or a session restore won't happen.
  virtual void StartScavengingUnusedSessionStorage() = 0;

 protected:
  virtual ~DOMStorageContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
