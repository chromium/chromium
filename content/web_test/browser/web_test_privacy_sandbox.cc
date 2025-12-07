// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_privacy_sandbox.h"

#include "base/base64.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebTestPrivacySandbox::WebTestPrivacySandbox(WebContents* web_contents)
    : WebContentsUserData<WebTestPrivacySandbox>(*web_contents) {}

WebTestPrivacySandbox::~WebTestPrivacySandbox() = default;

// static
WebTestPrivacySandbox* WebTestPrivacySandbox::GetOrCreate(
    WebContents* web_contents) {
  WebContentsUserData<WebTestPrivacySandbox>::CreateForWebContents(
      web_contents);
  return WebContentsUserData<WebTestPrivacySandbox>::FromWebContents(
      web_contents);
}

void WebTestPrivacySandbox::Bind(
    mojo::PendingReceiver<blink::test::mojom::WebPrivacySandboxAutomation>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void WebTestPrivacySandbox::SetProtectedAudienceKAnonymity(
    const url::Origin& owner_origin,
    const std::string& name,
    const std::vector<std::string>& hashes,
    SetProtectedAudienceKAnonymityCallback callback) {
  InterestGroupManagerImpl* manager =
      static_cast<InterestGroupManagerImpl*>(GetWebContents()
                                                 .GetBrowserContext()
                                                 ->GetDefaultStoragePartition()
                                                 ->GetInterestGroupManager());

  std::vector<std::string> byte_hashes;
  for (const auto& hash : hashes) {
    std::string byte_hash;
    if (!base::Base64Decode(hash, &byte_hash,
                            base::Base64DecodePolicy::kForgiving)) {
      mojo::ReportBadMessage("hash not base64 encoded");
      return;
    }
    byte_hashes.emplace_back(std::move(byte_hash));
  }

  if (manager) {
    manager->UpdateKAnonymity(blink::InterestGroupKey(owner_origin, name),
                              /*positive_hashed_keys=*/byte_hashes,
                              /*update_time=*/base::Time::Now(),
                              /*replace_existing_values=*/true);
  }
  std::move(callback).Run();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebTestPrivacySandbox);

}  // namespace content
