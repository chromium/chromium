// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_PRIVATE_STORAGE_HELPER_H_
#define CONTENT_BROWSER_PLUGIN_PRIVATE_STORAGE_HELPER_H_

#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error This file should only be included when plugins are enabled.
#endif

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
class FileSystemContext;
class SpecialStoragePolicy;
}

namespace content {

// Clear the plugin private filesystem data in |filesystem_context| for
// |storage_key| if any file has a last modified time between |begin|
// and |end|. If |storage_key| is not specified, then all available
// origins are checked. |callback| is called when the operation is complete.
// This must be called on the file task runner.
void ClearPluginPrivateDataOnFileTaskRunner(
    scoped_refptr<storage::FileSystemContext> filesystem_context,
    const blink::StorageKey& storage_key,
    StoragePartition::OriginMatcherFunction origin_matcher,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_PRIVATE_STORAGE_HELPER_H_
