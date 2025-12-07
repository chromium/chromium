// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_registry.h"

#include "content/browser/webid/identity_registry_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace content {

IdentityRegistry::IdentityRegistry(
    content::WebContents* web_contents,
    base::WeakPtr<IdentityRegistryDelegate> delegate,
    const GURL& idp_config_url)
    : content::WebContentsUserData<IdentityRegistry>(*web_contents),
      delegate_(delegate),
      idp_config_url_(idp_config_url) {}

IdentityRegistry::~IdentityRegistry() = default;

void IdentityRegistry::NotifyClose(const url::Origin& notifier_origin) {
  url::Origin idp_origin(url::Origin::Create(idp_config_url_));
  if (!idp_origin.IsSameOriginWith(notifier_origin) || !delegate_) {
    if (delegate_) {
      delegate_->OnOriginMismatch(IdentityRegistryDelegate::Method::kClose,
                                  idp_origin, notifier_origin);
    }
    return;
  }

  delegate_->OnClose();
}

bool IdentityRegistry::NotifyResolve(
    const url::Origin& notifier_origin,
    const std::optional<std::string>& account_id,
    const base::Value& token) {
  url::Origin idp_origin(url::Origin::Create(idp_config_url_));
  if (!idp_origin.IsSameOriginWith(notifier_origin) || !delegate_) {
    if (delegate_) {
      delegate_->OnOriginMismatch(IdentityRegistryDelegate::Method::kClose,
                                  idp_origin, notifier_origin);
    }
    return false;
  }

  return delegate_->OnResolve(idp_config_url_, account_id, token);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IdentityRegistry);

}  // namespace content
