// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_
#define CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_

#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {

class WebContents;
class MockIdentityRegistry;

// Stores a FederatedIdentityModalDialogViewDelegate which can later be
// retrieved using the same WebContents.
class CONTENT_EXPORT IdentityRegistry
    : public WebContentsUserData<IdentityRegistry> {
 public:
  ~IdentityRegistry() override;
  virtual void NotifyClose(const url::Origin& notifier_origin);
  virtual bool NotifyResolve(const url::Origin& notifier_origin,
                             const std::optional<std::string>& account_id,
                             const std::string& token);

 private:
  friend class content::WebContentsUserData<IdentityRegistry>;
  friend class content::MockIdentityRegistry;

  // An identity registry is constructed with a |web_contents| which the
  // registry is attached to, a |delegate| which is used to control modal dialog
  // views and an |idp_config_url| which is the URL for the IDP associated with
  // this registry. Same-origin checks happen against the origin of this URL.
  explicit IdentityRegistry(
      content::WebContents* web_contents,
      base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate,
      const GURL& idp_config_url);

  base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate_;
  GURL idp_config_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_
