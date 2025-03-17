// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_authentication_delegate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "build/buildflag.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/webauth/authenticator_environment.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

WebAuthenticationDelegate::WebAuthenticationDelegate() = default;

WebAuthenticationDelegate::~WebAuthenticationDelegate() = default;

bool WebAuthenticationDelegate::OverrideCallerOriginAndRelyingPartyIdValidation(
    BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  // Perform regular security checks for all origins and RP IDs.
  return false;
}

bool WebAuthenticationDelegate::OriginMayUseRemoteDesktopClientOverride(
    BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  // No origin is permitted to claim RP IDs on behalf of another origin.
  return false;
}

std::optional<std::string>
WebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  return std::nullopt;
}

bool WebAuthenticationDelegate::ShouldPermitIndividualAttestation(
    BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  return false;
}

bool WebAuthenticationDelegate::SupportsResidentKeys(
    RenderFrameHost* render_frame_host) {
#if !BUILDFLAG(IS_ANDROID)
  // The testing API supports resident keys, but for real requests //content
  // doesn't by default.
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  if (AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          frame_tree_node)) {
    return true;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return false;
}

bool WebAuthenticationDelegate::IsFocused(WebContents* web_contents) {
  return true;
}

void WebAuthenticationDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        RenderFrameHost* render_frame_host,
        base::OnceCallback<void(std::optional<bool>)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  if (AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          frame_tree_node)) {
    std::move(callback).Run(
        AuthenticatorEnvironment::GetInstance()
            ->HasVirtualUserVerifyingPlatformAuthenticator(frame_tree_node));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::nullopt);
}

WebAuthenticationRequestProxy* WebAuthenticationDelegate::MaybeGetRequestProxy(
    BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  return nullptr;
}

void WebAuthenticationDelegate::PasskeyUnrecognized(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::vector<uint8_t>& passkey_credential_id,
    const std::string& relying_party_id) {}

void WebAuthenticationDelegate::SignalAllAcceptedCredentials(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids) {}

void WebAuthenticationDelegate::UpdateUserPasskeys(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    std::vector<uint8_t>& user_id,
    const std::string& name,
    const std::string& display_name) {}

void WebAuthenticationDelegate::BrowserProvidedPasskeysAvailable(
    BrowserContext* browser_context,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

#if BUILDFLAG(IS_MAC)
std::optional<WebAuthenticationDelegate::TouchIdAuthenticatorConfig>
WebAuthenticationDelegate::GetTouchIdAuthenticatorConfig(
    BrowserContext* browser_context) {
  return std::nullopt;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
WebAuthenticationDelegate::ChromeOSGenerateRequestIdCallback
WebAuthenticationDelegate::GetGenerateRequestIdCallback(
    RenderFrameHost* render_frame_host) {
  return base::NullCallback();
}
#endif

}  // namespace content
