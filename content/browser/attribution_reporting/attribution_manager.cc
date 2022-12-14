// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager.h"

#include "base/check.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace content {

// static
AttributionManager* AttributionManager::FromWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  return static_cast<StoragePartitionImpl*>(
             web_contents->GetBrowserContext()->GetDefaultStoragePartition())
      ->GetAttributionManager();
}

// static
attribution_reporting::mojom::OsSupport AttributionManager::GetOsSupport() {
  return AttributionManagerImpl::GetOsSupport();
}

}  // namespace content
