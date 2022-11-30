// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_UTILS_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_UTILS_H_

#include "url/gurl.h"

namespace storage {

namespace BlobUrlUtils {
// We can't use GURL directly for these hash fragment manipulations
// since it doesn't have specific knowlege of the BlobURL format. GURL
// treats BlobURLs as if they were PathURLs which don't support hash
// fragments.

// Returns true iff |url| contains a hash fragment.
bool UrlHasFragment(const GURL& url);

// Returns |url| with any potential hash fragment stripped. If |url| didn't
// contain such a fragment this returns |url| unchanged.
GURL ClearUrlFragment(const GURL& url);

}  // namespace BlobUrlUtils
}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_UTILS_H_