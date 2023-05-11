// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_
#define CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_

#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

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
                             const std::string& token);

 private:
  friend class content::WebContentsUserData<IdentityRegistry>;
  friend class content::MockIdentityRegistry;

  // An identity registry is constructed with a |web_contents| which the
  // registry is attached to, a |delegate| which is used to control modal dialog
  // views and a |registry_origin| which is the origin of this constructor's
  // caller.
  explicit IdentityRegistry(content::WebContents* web_contents,
                            FederatedIdentityModalDialogViewDelegate* delegate,
                            const url::Origin& registry_origin);

  raw_ptr<FederatedIdentityModalDialogViewDelegate> delegate_;
  url::Origin registry_origin_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_H_
