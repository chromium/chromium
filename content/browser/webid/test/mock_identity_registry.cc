// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/mock_identity_registry.h"

#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "url/gurl.h"

namespace content {

MockIdentityRegistry::MockIdentityRegistry(
    content::WebContents* web_contents,
    base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate,
    const GURL& idp_config_url)
    : IdentityRegistry(web_contents, delegate, idp_config_url) {}

MockIdentityRegistry::~MockIdentityRegistry() = default;

}  // namespace content
