// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_registry.h"

#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace content {

IdentityRegistry::IdentityRegistry(
    content::WebContents* web_contents,
    FederatedIdentityModalDialogViewDelegate* delegate,
    const url::Origin& registry_origin)
    : content::WebContentsUserData<IdentityRegistry>(*web_contents),
      delegate_(std::move(delegate)),
      registry_origin_(registry_origin) {}

IdentityRegistry::~IdentityRegistry() = default;

void IdentityRegistry::NotifyClose(const url::Origin& notifier_origin) {
  if (!registry_origin_.IsSameOriginWith(notifier_origin)) {
    return;
  }

  delegate_->NotifyClose();
}

bool IdentityRegistry::NotifyResolve(const url::Origin& notifier_origin,
                                     const std::string& token) {
  if (!registry_origin_.IsSameOriginWith(notifier_origin)) {
    return false;
  }
  return delegate_->NotifyResolve(token);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IdentityRegistry);

}  // namespace content
