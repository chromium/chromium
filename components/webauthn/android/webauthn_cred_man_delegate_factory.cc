// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webauthn {

WebAuthnCredManDelegateFactory::WebAuthnCredManDelegateFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebAuthnCredManDelegateFactory>(
          *web_contents) {}

WebAuthnCredManDelegateFactory::~WebAuthnCredManDelegateFactory() = default;

// static
WebAuthnCredManDelegateFactory* WebAuthnCredManDelegateFactory::GetFactory(
    content::WebContents* web_contents) {
  // This does nothing if it already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

WebAuthnCredManDelegate* WebAuthnCredManDelegateFactory::GetRequestDelegate(
    content::RenderFrameHost* frame_host) {
  CHECK_EQ(web_contents(),
           content::WebContents::FromRenderFrameHost(frame_host));

  // A RenderFrameHost without a live RenderFrame will never call
  // RenderFrameDeleted(), which would leave the new delegate in the map for
  // the lifetime of the WebContents.
  if (!frame_host->IsRenderFrameLive()) {
    return nullptr;
  }

  auto it = delegate_map_.find(frame_host);
  if (it == delegate_map_.end()) {
    auto [new_it, inserted] = delegate_map_.try_emplace(
        frame_host, std::make_unique<WebAuthnCredManDelegate>(web_contents()));
    it = new_it;
  }
  return it->second.get();
}

void WebAuthnCredManDelegateFactory::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  delegate_map_.erase(frame_host);
}

void WebAuthnCredManDelegateFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // A frame navigation loses both autofill and WebAuthn state.
  delegate_map_.erase(navigation_handle->GetRenderFrameHost());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAuthnCredManDelegateFactory);

}  // namespace webauthn
