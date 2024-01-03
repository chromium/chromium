// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_storage_access_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#include <list>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace content {

WebTestStorageAccessManager::WebTestStorageAccessManager(
    BrowserContext* browser_context)
    : browser_context_(
          base::raw_ref<content::BrowserContext>::from_ptr(browser_context)) {}

WebTestStorageAccessManager::~WebTestStorageAccessManager() = default;

void WebTestStorageAccessManager::SetStorageAccess(
    const std::string& origin,
    const std::string& embedding_origin,
    const bool blocked,
    blink::test::mojom::StorageAccessAutomation::SetStorageAccessCallback
        callback) {
  CHECK(callback);
  if (!ContentSettingsPattern::FromString(origin).IsValid() ||
      !ContentSettingsPattern::FromString(embedding_origin).IsValid()) {
    std::move(callback).Run(false);
    return;
  }

  // Note: we're intentionally ignoring the `origin` and `embedding_origin`
  // patterns here, aside from checking that they are valid patterns. Chromium's
  // Storage Access API implementation intentionally does not override
  // site-specific cookie settings exceptions, so it is incorrect (or at best,
  // unhelpful) to use the patterns to set a site-specific setting.
  //
  // We've proposed an update to the spec based on this limitation:
  // https://github.com/privacycg/storage-access/issues/162.

  // Enable third-party cookies blocking if needed, otherwise disable. This will
  // cause Storage Access API grants to become relevant.
  StoragePartition* storage_partition =
      browser_context_->GetDefaultStoragePartition();
  CHECK(storage_partition);
  network::mojom::CookieManager* cookie_manager =
      storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  cookie_manager->BlockThirdPartyCookies(blocked);
  storage_partition->FlushNetworkInterfaceForTesting();
  std::move(callback).Run(true);
}

void WebTestStorageAccessManager::Bind(
    mojo::PendingReceiver<blink::test::mojom::StorageAccessAutomation>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace content
