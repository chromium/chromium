// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_provider.h"

#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

class AttributionManagerProviderImpl : public AttributionManagerProvider {
 public:
  AttributionManagerProviderImpl() = default;

  ~AttributionManagerProviderImpl() override = default;

  AttributionManagerProviderImpl(const AttributionManagerProviderImpl&) =
      delete;
  AttributionManagerProviderImpl(AttributionManagerProviderImpl&&) = delete;

  AttributionManagerProviderImpl& operator=(
      const AttributionManagerProviderImpl&) = delete;
  AttributionManagerProviderImpl& operator=(AttributionManagerProviderImpl&&) =
      delete;

 private:
  // AttributionManagerProvider:
  AttributionManager* GetManager(WebContents* web_contents) const override {
    return static_cast<StoragePartitionImpl*>(
               web_contents->GetBrowserContext()->GetDefaultStoragePartition())
        ->GetAttributionManager();
  }
};

}  // namespace

// static
std::unique_ptr<AttributionManagerProvider>
AttributionManagerProvider::Default() {
  return std::make_unique<AttributionManagerProviderImpl>();
}

}  // namespace content
