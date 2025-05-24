// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_DELEGATE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/buildflag.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC))

namespace content {

// WebAuthenticationDelegate is an interface that lets the //content layer
// provide embedder specific configuration for handling Web Authentication API
// (https://www.w3.org/TR/webauthn/) requests.
//
// Instances can be obtained via
// ContentBrowserClient::GetWebAuthenticationDelegate().
class CONTENT_EXPORT WebAuthenticationDelegate {
 public:
  WebAuthenticationDelegate();
  virtual ~WebAuthenticationDelegate();

  // Returns true if `caller_origin` should be able to claim the given Relying
  // Party ID outside of regular processing. Otherwise, standard WebAuthn RP ID
  // security checks are performed by `WebAuthRequestSecurityChecker`.
  // (https://www.w3.org/TR/2021/REC-webauthn-2-20210408/#relying-party-identifier).
  //
  // This is an access-control decision: RP IDs are used to control access to
  // credentials so thought is required before allowing an origin to assert an
  // RP ID.
  virtual bool OverrideCallerOriginAndRelyingPartyIdValidation(
      BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id);

  // Returns whether |caller_origin| is permitted to use the
  // RemoteDesktopClientOverride extension.
  //
  // This is an access control decision: RP IDs are used to control access to
  // credentials. If this method returns true, the respective origin is able to
  // claim any RP ID.
  virtual bool OriginMayUseRemoteDesktopClientOverride(
      BrowserContext* browser_context,
      const url::Origin& caller_origin);

  // Permits the embedder to override the Relying Party ID for a WebAuthn call,
  // given the claimed relying party ID and the origin of the caller.
  //
  // This is an access-control decision: RP IDs are used to control access to
  // credentials so thought is required before allowing an origin to assert an
  // RP ID. RP ID strings may be stored on authenticators and may later appear
  // in management UI.
  virtual std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin);

  // Returns true if the given relying party ID is permitted to receive
  // individual attestation certificates. This:
  //  a) triggers a signal to the security key that returning individual
  //     attestation certificates is permitted, and
  //  b) skips any permission prompt for attestation.
  virtual bool ShouldPermitIndividualAttestation(
      BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id);

  // SupportsResidentKeys returns true if this implementation of
  // |AuthenticatorRequestClientDelegate| supports resident keys for WebAuthn
  // requests originating from |render_frame_host|. If false then requests to
  // create or get assertions will be immediately rejected.
  virtual bool SupportsResidentKeys(RenderFrameHost* render_frame_host);

  // Returns whether |web_contents| is the active tab in the focused window. We
  // do not want to allow authenticatorMakeCredential operations to be triggered
  // by background tabs.
  //
  // Note that the default implementation of this function, and the
  // implementation in ChromeContentBrowserClient for Android, return |true| so
  // that testing is possible.
  virtual bool IsFocused(WebContents* web_contents);

  // Determines if the isUserVerifyingPlatformAuthenticator API call originating
  // from |render_frame_host| should be overridden with a value. The callback is
  // invoked with the override value, or with std::nullopt if it should not be
  // overridden. The callback can be invoked synchronously or asynchronously.
  virtual void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost* render_frame_host,
      base::OnceCallback<void(std::optional<bool>)> callback);

  // Returns the active WebAuthenticationRequestProxy for WebAuthn requests
  // originating from `caller_origin` in `browser_context`.
  //
  // If this method returns a proxy, the caller is expected to hand off WebAuthn
  // request handling to this proxy instance.
  virtual WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      BrowserContext* browser_context,
      const url::Origin& caller_origin);

  // PasskeyUnrecognized is called when a relying party notifies Chrome that a
  // passkey was not recognized. This may remove or hide a passkey from the
  // storage provider.
  virtual void PasskeyUnrecognized(
      content::WebContents* web_contents,
      const url::Origin& origin,
      const std::vector<uint8_t>& passkey_credential_id,
      const std::string& relying_party_id);

  // SignalAllAcceptedCredentials removes any non-appearing credential in the
  // all_accepted_credentials_ids list from the credential storage provider for
  // the given relying party ID and user ID.
  // If passkey hiding is enabled, non-appearing credentials are hidden instead.
  // Credentials appearing in the list that have previously been hidden are
  // restored.
  virtual void SignalAllAcceptedCredentials(
      content::WebContents* web_contents,
      const url::Origin& origin,
      const std::string& relying_party_id,
      const std::vector<uint8_t>& user_id,
      const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids);

  // UpdateUserPasskeys updates the name and display name of a passkey for the
  // given relying party ID and user ID.
  virtual void UpdateUserPasskeys(content::WebContents* web_contents,
                                  const url::Origin& origin,
                                  const std::string& relying_party_id,
                                  std::vector<uint8_t>& user_id,
                                  const std::string& name,
                                  const std::string& display_name);

  // Invokes the callback with true when passkeys provided by browser sync
  // are available for use, and false otherwise. The callback can be invoked
  // synchronously or asynchronously.
  virtual void BrowserProvidedPasskeysAvailable(
      BrowserContext* browser_context,
      base::OnceCallback<void(bool)> callback);

#if BUILDFLAG(IS_MAC)
  using TouchIdAuthenticatorConfig = device::fido::mac::AuthenticatorConfig;

  // Returns configuration data for the built-in Touch ID platform
  // authenticator. May return nullopt if the authenticator is not available in
  // the current context, in which case the Touch ID authenticator will be
  // unavailable.
  virtual std::optional<TouchIdAuthenticatorConfig>
  GetTouchIdAuthenticatorConfig(BrowserContext* browser_context);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // Callback that should generate and return a unique request id.
  using ChromeOSGenerateRequestIdCallback =
      base::RepeatingCallback<std::string()>;

  // Returns a callback to generate a request id for a WebAuthn request
  // originating from |RenderFrameHost|. The request id has two purposes: 1.
  // ChromeOS UI will use the request id to find the source window and show a
  // dialog accordingly; 2. The authenticator will include the request id when
  // asking ChromeOS platform to cancel the request.
  virtual ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      RenderFrameHost* render_frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_AUTHENTICATION_DELEGATE_H_
