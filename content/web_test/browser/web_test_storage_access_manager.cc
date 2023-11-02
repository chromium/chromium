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

#include "base/bind.h"
#include "base/callback.h"

namespace content {

WebTestStorageAccessManager::WebTestStorageAccessManager(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

WebTestStorageAccessManager::~WebTestStorageAccessManager() = default;

void WebTestStorageAccessManager::SetStorageAccess(
    const std::string& origin,
    const std::string& embedding_origin,
    const bool blocked,
    blink::test::mojom::StorageAccessAutomation::SetStorageAccessCallback
        callback) {
  const ContentSetting setting =
      blocked ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;

  auto primary_pattern = ContentSettingsPattern::FromString(origin);
  if (!primary_pattern.IsValid()) {
    std::move(callback).Run(false);
    return;
  }

  auto secondary_pattern = ContentSettingsPattern::FromString(embedding_origin);
  if (!secondary_pattern.IsValid()) {
    std::move(callback).Run(false);
    return;
  }

  content_settings_for_automation_.push_back(
      ContentSettingPatternSource(primary_pattern, secondary_pattern,
                                  base::Value(setting), std::string(), false));

  // TODO(https://crbug.com/1106098) - Storage Access API should support all
  // storage types in content shell

  // Storage access API (SAA) settings for cookies are implemented in the
  // network::CookieSettings class. Settings for other storage types such as
  // local storage and indexeddb are implemented in
  // content_settings::CookieSettings. Content Shell does not
  // use the content_settings::CookieSettings class so SAA affects only
  // cookie access here. Other storage types are always allowed in
  // Content Shell.

  // Since cookies are the only storage type governed by SAA in Content Shell,
  // this class handles cookie rules only. If Content Shell or SAA are
  // updated in the future so that more storage types are governed by SAA in
  // Content Shell, then we should update this class to handle those other
  // types are well.

  auto* storage_partition = browser_context_->GetDefaultStoragePartition();
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();

  // Enable third-party cookies blocking if we have not done so yet. This will
  // cause the content settings to take effect.
  if (!third_party_cookies_blocked_) {
    cookie_manager->BlockThirdPartyCookies(true);
    third_party_cookies_blocked_ = true;
  }

  // Update the cookie manager's copy of the content settings.
  cookie_manager->SetContentSettings(content_settings_for_automation_);
  std::move(callback).Run(true);
}

void WebTestStorageAccessManager::Bind(
    mojo::PendingReceiver<blink::test::mojom::StorageAccessAutomation>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace content
