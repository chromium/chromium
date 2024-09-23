// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager.h"

#include "base/check.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

// static
AttributionManager* AttributionManager::FromWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  return FromBrowserContext(web_contents->GetBrowserContext());
}

// static
AttributionManager* AttributionManager::FromBrowserContext(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  return static_cast<StoragePartitionImpl*>(
             browser_context->GetDefaultStoragePartition())
      ->GetAttributionManager();
}

// static
network::mojom::AttributionSupport AttributionManager::GetAttributionSupport(
    bool client_os_disabled) {
  return GetContentClient()->browser()->GetAttributionSupport(
      AttributionOsLevelManager::GetApiState(), client_os_disabled);
}

}  // namespace content
