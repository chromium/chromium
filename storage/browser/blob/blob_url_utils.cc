// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_utils.h"

#include "base/containers/contains.h"

namespace storage::BlobUrlUtils {

bool UrlHasFragment(const GURL& url) {
  return base::Contains(url.spec(), '#');
}

GURL ClearUrlFragment(const GURL& url) {
  size_t hash_pos = url.spec().find('#');
  if (hash_pos == std::string::npos)
    return url;
  return GURL(url.spec().substr(0, hash_pos));
}

}  // namespace storage::BlobUrlUtils
