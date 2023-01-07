// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/session_storage_namespace.h"

namespace content {

SessionStorageNamespaceMap CreateMapWithDefaultSessionStorageNamespace(
    BrowserContext* browser_context,
    scoped_refptr<SessionStorageNamespace> session_storage_namespace) {
  SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[StoragePartitionConfig::CreateDefault(
      browser_context)] = session_storage_namespace;
  return session_storage_namespace_map;
}

}  // namespace content
