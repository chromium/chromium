// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_REQUEST_INFO_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_REQUEST_INFO_H_

#include "base/component_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

// FileSystemRequestInfo is a struct containing the information
// necessary for a FileSystemURLLoaderFactory to mount the
// FileSystem requested by the caller.
struct COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemRequestInfo {
  // The original request URL (always set).
  GURL url;
  // The storage domain (always set).
  std::string storage_domain;
  // Set by the network service for use by callbacks.
  // TODO(https://crbug.com/364652019): Do something about this. This is really
  // a content::FrameTreeNodeId, but DEPS don't allow it to be correctly typed.
  // This is used to smuggle a FrameTreeNodeId from content/ to chrome/ in
  // violation of layering practices.
  int content_id = 0;
  // The original request blink::StorageKey (always set).
  blink::StorageKey storage_key;

  FileSystemRequestInfo(const GURL& url,
                        const std::string& storage_domain,
                        int content_id,
                        const blink::StorageKey& storage_key);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_REQUEST_INFO_H_
