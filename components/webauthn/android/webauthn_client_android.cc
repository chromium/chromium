// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_client_android.h"

#include <memory>

#include "base/check.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/web_contents.h"

namespace components {

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
    base::RepeatingClosure full_assertion_request) {
  auto* cred_man_delegate = WebAuthnCredManDelegate::GetRequestDelegate(
      content::WebContents::FromRenderFrameHost(render_frame_host));
  cred_man_delegate->OnCredManConditionalRequestPending(
      render_frame_host, has_results, std::move(full_assertion_request));
  return;
}

void WebAuthnClientAndroid::OnCredManUiClosed(
    content::RenderFrameHost* render_frame_host,
    bool success) {
  auto* cred_man_delegate = WebAuthnCredManDelegate::GetRequestDelegate(
      content::WebContents::FromRenderFrameHost(render_frame_host));
  cred_man_delegate->OnCredManUiClosed(success);
  return;
}

}  // namespace components
