// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace webauthn {

class WebAuthnCredManDelegate;

class WebAuthnCredManDelegateFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebAuthnCredManDelegateFactory> {
 public:
  WebAuthnCredManDelegateFactory(const WebAuthnCredManDelegateFactory&) =
      delete;
  WebAuthnCredManDelegateFactory& operator=(
      const WebAuthnCredManDelegateFactory&) = delete;

  ~WebAuthnCredManDelegateFactory() override;

  // Returns the instance of WebAuthnCredManDelegateFactory for the
  // given |web_contents|, creating one if it doesn't already exist.
  static WebAuthnCredManDelegateFactory* GetFactory(
      content::WebContents* web_contents);

  // Returns the delegate for the given frame, creating one if possible.
  // Can return nullptr.
  WebAuthnCredManDelegate* GetRequestDelegate(
      content::RenderFrameHost* frame_host);

 private:
  friend class content::WebContentsUserData<WebAuthnCredManDelegateFactory>;
  friend class WebAuthnCredManDelegateFactoryTestApi;

  explicit WebAuthnCredManDelegateFactory(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<WebAuthnCredManDelegate>>
      delegate_map_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_H_
