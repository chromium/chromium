// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_client_android.h"

#include <memory>

#include "base/check.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "content/public/browser/web_contents.h"

namespace webauthn {

// The WebAuthnClientAndroid instance, which is set by the embedder.
WebAuthnClientAndroid* g_webauthn_client = nullptr;

WebAuthnClientAndroid::~WebAuthnClientAndroid() = default;

// static
void WebAuthnClientAndroid::SetClient(
    std::unique_ptr<WebAuthnClientAndroid> client) {
  DCHECK(client);
  DCHECK(!g_webauthn_client);
  g_webauthn_client = client.release();
}

// static
WebAuthnClientAndroid* WebAuthnClientAndroid::GetClient() {
  DCHECK(g_webauthn_client);
  return g_webauthn_client;
}

void WebAuthnClientAndroid::OnCredManConditionalRequestPending(
    content::RenderFrameHost* render_frame_host,
    bool has_results,
    base::RepeatingCallback<void(bool)> full_assertion_request) {
  if (webauthn::WebAuthnCredManDelegate* cred_man_delegate =
          webauthn::WebAuthnCredManDelegateFactory::GetFactory(
              content::WebContents::FromRenderFrameHost(render_frame_host))
              ->GetRequestDelegate(render_frame_host)) {
    cred_man_delegate->OnCredManConditionalRequestPending(
        has_results, std::move(full_assertion_request));
  }
}

void WebAuthnClientAndroid::OnCredManUiClosed(
    content::RenderFrameHost* render_frame_host,
    bool success) {
  if (webauthn::WebAuthnCredManDelegate* cred_man_delegate =
          webauthn::WebAuthnCredManDelegateFactory::GetFactory(
              content::WebContents::FromRenderFrameHost(render_frame_host))
              ->GetRequestDelegate(render_frame_host)) {
    cred_man_delegate->OnCredManUiClosed(success);
  }
}

void WebAuthnClientAndroid::CleanupCredManRequest(
    content::RenderFrameHost* frame_host) {
  if (webauthn::WebAuthnCredManDelegate* credman_delegate =
          webauthn::WebAuthnCredManDelegateFactory::GetFactory(
              content::WebContents::FromRenderFrameHost(frame_host))
              ->GetRequestDelegate(frame_host)) {
    credman_delegate->CleanUpConditionalRequest();
  }
  return;
}

void WebAuthnClientAndroid::OnPasswordCredentialReceived(
    content::RenderFrameHost* render_frame_host,
    std::u16string username,
    std::u16string password) {
  if (webauthn::WebAuthnCredManDelegate* cred_man_delegate =
          webauthn::WebAuthnCredManDelegateFactory::GetFactory(
              content::WebContents::FromRenderFrameHost(render_frame_host))
              ->GetRequestDelegate(render_frame_host)) {
    cred_man_delegate->FillUsernameAndPassword(username, password);
  }
}

}  // namespace webauthn
