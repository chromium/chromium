// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/ref_counted.h"

namespace content {

// This is a ref-counted class that represents a SessionStorageNamespace.
// On destruction it ensures that the storage namespace is destroyed.
class SessionStorageNamespace
    : public base::RefCountedThreadSafe<SessionStorageNamespace> {
 public:
  // Returns the ID for the |SessionStorageNamespace|. The ID is unique among
  // all SessionStorageNamespace objects and across browser runs.
  virtual const std::string& id() = 0;

  // For marking that the sessionStorage will be needed or won't be needed by
  // session restore.
  virtual void SetShouldPersist(bool should_persist) = 0;

  virtual bool should_persist() = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionStorageNamespace>;
  virtual ~SessionStorageNamespace() {}
};

// Used to store mappings of StoragePartition id to SessionStorageNamespace.
typedef std::map<std::string, scoped_refptr<SessionStorageNamespace> >
    SessionStorageNamespaceMap;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
