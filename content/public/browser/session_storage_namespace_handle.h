// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_HANDLE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {

// This is a ref-counted class that represents a
// `SessionStorageNamespaceHandle`. On destruction it ensures that the storage
// namespace is destroyed.
class SessionStorageNamespaceHandle
    : public base::RefCountedThreadSafe<SessionStorageNamespaceHandle> {
 public:
  // Returns the session storage namespace ID. The ID is unique among session
  // storage namespaces and across browser runs.
  virtual const std::string& id() = 0;

  // For marking that the sessionStorage will be needed or won't be needed by
  // session restore.
  virtual void SetShouldPersist(bool should_persist) = 0;

  virtual bool should_persist() = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionStorageNamespaceHandle>;
  virtual ~SessionStorageNamespaceHandle() {}
};

// Used to store mappings of `StoragePartitionConfig` to
// `SessionStorageNamespaceHandle`.
typedef std::map<StoragePartitionConfig,
                 scoped_refptr<SessionStorageNamespaceHandle>>
    SessionStorageNamespaceHandleMap;

// Helper function that creates a `SessionStorageNamespaceHandleMap` and assigns
// `session_storage_namespace` to the default `StoragePartitionConfig`.
CONTENT_EXPORT SessionStorageNamespaceHandleMap
CreateMapWithDefaultSessionStorageNamespace(
    BrowserContext* browser_context,
    scoped_refptr<SessionStorageNamespaceHandle> session_storage_namespace);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_HANDLE_H_
